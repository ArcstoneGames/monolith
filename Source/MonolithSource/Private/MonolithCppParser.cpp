#include "MonolithCppParser.h"
#include "MonolithUEPreprocessor.h"
#include "MonolithSourceDatabase.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Regex.h"

namespace MonolithCppParserDetail
{
	/**
	 * Walk forward from (StartLine, StartCol) — which must sit on an opening '(' —
	 * until the parentheses balance. Text after a `//` on a line is ignored. Returns
	 * false when the parens do not balance within MaxLines. On success OutLine/OutCol
	 * address the character just past the matching ')'.
	 */
	static bool FindMatchingParen(const TArray<FString>& Lines, int32 StartLine, int32 StartCol,
		int32 MaxLines, int32& OutLine, int32& OutCol)
	{
		int32 Depth = 0;
		const int32 LastLine = FMath::Min(Lines.Num() - 1, StartLine + MaxLines);
		for (int32 L = StartLine; L <= LastLine; ++L)
		{
			const FString& Text = Lines[L];
			for (int32 C = (L == StartLine ? StartCol : 0); C < Text.Len(); ++C)
			{
				const TCHAR Ch = Text[C];
				if (Ch == TEXT('/') && C + 1 < Text.Len() && Text[C + 1] == TEXT('/'))
				{
					break; // rest of the line is a comment
				}
				if (Ch == TEXT('('))
				{
					++Depth;
				}
				else if (Ch == TEXT(')'))
				{
					if (--Depth == 0)
					{
						OutLine = L;
						OutCol = C + 1;
						return true;
					}
				}
			}
		}
		return false;
	}

	/** Net '(' minus ')' count on one line, ignoring anything after `//`. */
	static int32 ParenBalance(const FString& Text)
	{
		int32 Balance = 0;
		for (int32 C = 0; C < Text.Len(); ++C)
		{
			const TCHAR Ch = Text[C];
			if (Ch == TEXT('/') && C + 1 < Text.Len() && Text[C + 1] == TEXT('/'))
			{
				break;
			}
			if (Ch == TEXT('(')) { ++Balance; }
			else if (Ch == TEXT(')')) { --Balance; }
		}
		return Balance;
	}

	/** Strip a trailing `// comment` from a line. */
	static FString StripLineComment(const FString& Text)
	{
		const int32 Idx = Text.Find(TEXT("//"), ESearchCase::CaseSensitive);
		return Idx == INDEX_NONE ? Text : Text.Left(Idx);
	}

	/**
	 * True when Tail (the text between a function's closing ')' and its body '{')
	 * holds only trailing function qualifiers: const, noexcept, override, final,
	 * volatile, ref-qualifiers, or a trailing return type (`-> T`).
	 */
	static bool IsQualifierTail(const FString& Tail)
	{
		static const TCHAR* Qualifiers[] = {
			TEXT("const"), TEXT("noexcept"), TEXT("override"), TEXT("final"), TEXT("volatile"),
		};

		FString Rest = Tail.TrimStartAndEnd();
		while (!Rest.IsEmpty())
		{
			bool bConsumed = false;
			if (Rest.StartsWith(TEXT("&")))
			{
				Rest = Rest.Mid(1).TrimStart();
				continue;
			}
			if (Rest.StartsWith(TEXT("->")))
			{
				// Trailing return type: accept identifier/template/pointer characters only.
				for (int32 C = 2; C < Rest.Len(); ++C)
				{
					const TCHAR Ch = Rest[C];
					if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT(':') || Ch == TEXT('<')
						|| Ch == TEXT('>') || Ch == TEXT('*') || Ch == TEXT('&') || Ch == TEXT(',')
						|| FChar::IsWhitespace(Ch)))
					{
						return false;
					}
				}
				return true;
			}
			for (const TCHAR* Q : Qualifiers)
			{
				const int32 QLen = FCString::Strlen(Q);
				if (Rest.StartsWith(Q, ESearchCase::CaseSensitive)
					&& (Rest.Len() == QLen || !(FChar::IsAlnum(Rest[QLen]) || Rest[QLen] == TEXT('_'))))
				{
					Rest = Rest.Mid(QLen).TrimStart();
					bConsumed = true;
					break;
				}
			}
			if (!bConsumed)
			{
				return false;
			}
		}
		return true;
	}

	/** True when a qualified name is a constructor or destructor (`A::B::B` / `A::~A`). */
	static bool IsQualifiedCtorOrDtor(const FString& QualifiedName)
	{
		TArray<FString> Parts;
		QualifiedName.ParseIntoArray(Parts, TEXT("::"), true);
		if (Parts.Num() < 2)
		{
			return false;
		}
		FString Last = Parts.Last();
		Last.RemoveFromStart(TEXT("~"));
		return Last == Parts[Parts.Num() - 2];
	}
}

// ============================================================
// ParseFile — main entry point
// ============================================================

FParsedFileResult FMonolithCppParser::ParseFile(const FString& FilePath)
{
	FParsedFileResult Result;
	Result.FilePath = FilePath;

	// Load file
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *FilePath))
	{
		UE_LOG(LogMonolithSource, Warning, TEXT("CppParser: Failed to read file: %s"), *FilePath);
		return Result;
	}

	// Store original lines (1-based access: SourceLines[0] = line 1)
	Source.ParseIntoArray(Result.SourceLines, TEXT("\n"), false);

	// Preprocess — strip UE macros for clean regex matching
	FString CleanSource = MonolithUEPreprocessor::PreprocessSource(Source);
	TArray<FString> CleanLines;
	CleanSource.ParseIntoArray(CleanLines, TEXT("\n"), false);

	// Extraction passes
	ExtractIncludes(Source, Result);
	ExtractClassesAndStructs(Result.SourceLines, CleanLines, Result);
	ExtractEnums(Result.SourceLines, CleanLines, Result);
	ExtractFreeFunctions(Result.SourceLines, CleanLines, Result);
	ExtractMacrosAndTypedefs(Result.SourceLines, Result);

	return Result;
}

// ============================================================
// ExtractIncludes
// ============================================================

void FMonolithCppParser::ExtractIncludes(const FString& Source, FParsedFileResult& Result)
{
	FRegexPattern Pattern(TEXT("^\\s*#include\\s+[\"<]([^\">]+)[\">]"));

	for (const FString& Line : Result.SourceLines)
	{
		FRegexMatcher Matcher(Pattern, Line);
		if (Matcher.FindNext())
		{
			Result.Includes.Add(Matcher.GetCaptureGroup(1));
		}
	}
}

// ============================================================
// ExtractClassesAndStructs
// ============================================================

void FMonolithCppParser::ExtractClassesAndStructs(const TArray<FString>& OrigLines, const TArray<FString>& CleanLines, FParsedFileResult& Result)
{
	// Track names already found to avoid duplicates between Phase A and B
	TSet<FString> FoundNames;

	const int32 NumLines = CleanLines.Num();

	// Class/struct declaration pattern (applied to clean lines)
	FRegexPattern DeclPattern(TEXT("\\b(class|struct)\\s+(?:[A-Z][A-Z0-9]*_API\\s+)?(\\w+)(?:\\s*(?:final|sealed))?\\s*(?::\\s*(.+?))?\\s*\\{?\\s*$"));

	// ---- Phase A: UE-decorated classes ----
	for (int32 i = 0; i < OrigLines.Num(); ++i)
	{
		FString Trimmed = OrigLines[i].TrimStartAndEnd();
		if (!Trimmed.StartsWith(TEXT("UCLASS(")) && !Trimmed.StartsWith(TEXT("USTRUCT(")) && !Trimmed.StartsWith(TEXT("UINTERFACE(")))
		{
			continue;
		}

		// Look ahead up to 5 lines in clean lines for the declaration
		for (int32 j = i; j < FMath::Min(i + 6, NumLines); ++j)
		{
			FRegexMatcher Matcher(DeclPattern, CleanLines[j]);
			if (!Matcher.FindNext())
			{
				continue;
			}

			FParsedSourceSymbol Sym;
			Sym.Kind = Matcher.GetCaptureGroup(1);
			Sym.Name = Matcher.GetCaptureGroup(2);
			Sym.bIsUEMacro = true;
			Sym.LineStart = j + 1; // 1-based

			// Base classes
			FString InheritClause = Matcher.GetCaptureGroup(3);
			if (!InheritClause.IsEmpty())
			{
				ParseBaseClasses(InheritClause, Sym.BaseClasses);
			}

			// Docstring (skip UE macro line)
			Sym.Docstring = ExtractDocstring(OrigLines, i, true);

			// Find opening brace — could be on this line or next
			int32 BraceLineIdx = -1;
			for (int32 k = j; k < FMath::Min(j + 3, NumLines); ++k)
			{
				if (CleanLines[k].Contains(TEXT("{")))
				{
					BraceLineIdx = k;
					break;
				}
			}

			// Defensive parity with Phase B: FindClosingBrace returns Lines.Num() (a
			// sentinel COUNT, not a valid index) for an unterminated body at/near EOF.
			// Layer 1's clamp already makes ExtractMembers crash-proof, but treating an
			// unterminated body as no-body here avoids feeding it a past-array range at
			// all. Zero behavior change for well-formed files (CloseLine < Num there).
			const int32 CloseLine = (BraceLineIdx >= 0) ? FindClosingBrace(CleanLines, BraceLineIdx) : -1;
			const bool bHasBody = (BraceLineIdx >= 0) && (CloseLine < NumLines);

			if (bHasBody)
			{
				Sym.LineEnd = CloseLine;

				// Default access: struct=public, class=private
				FString DefaultAccess = Sym.Kind == TEXT("struct") ? TEXT("public") : TEXT("private");

				// Build signature from original lines
				Sym.Signature = OrigLines[j].TrimStartAndEnd();

				FoundNames.Add(Sym.Name);
				Result.Symbols.Add(MoveTemp(Sym));

				// Extract members within the body. COPY the parent name first — passing
				// Result.Symbols.Last().Name by reference is a use-after-free: ExtractMembers
				// Adds member symbols to Result.Symbols, and a reallocation frees the backing
				// store the reference points into (AV when the next member reads ParentClass).
				//
				// BraceLineIdx is a 0-based index and ExtractMembers takes 1-based lines, so
				// the first line AFTER the `{` is BraceLineIdx + 2 (same as Phase B). Passing
				// BraceLineIdx + 1 started the scan ON the `{` line: its brace pushed
				// ExtractMembers' depth to 1, the matching `}` lies past BodyEndLine, and
				// every member of every UCLASS/USTRUCT/UINTERFACE body was skipped (issue #156).
				const FString ParentName = Result.Symbols.Last().Name;
				ExtractMembers(OrigLines, BraceLineIdx + 2, CloseLine - 1,
					ParentName, DefaultAccess, Result);
			}
			else
			{
				// Forward declaration or no body — still record it
				Sym.LineEnd = j + 1;
				Sym.Signature = OrigLines[j].TrimStartAndEnd();
				FoundNames.Add(Sym.Name);
				Result.Symbols.Add(MoveTemp(Sym));
			}

			break; // found the declaration for this UE macro
		}
	}

	// ---- Phase B: Non-UE classes/structs ----
	// Allman-tolerant: the opening brace may be on the declaration line OR on a
	// following line (Epic coding standard). The trailing group is a `{`-OR-end-of-line
	// alternation, so BOTH forms match:
	//   allman:   `class ENGINE_API FFoo`           (brace on the next line)
	//   K&R/inline:`struct FFoo {};`                 (today's 87-style one-liners — no regression)
	// The `^\s*` line anchor is RETAINED — Phase B scans every line, so the anchor is
	// what keeps `template<class T>` and other mid-line matches out. The inheritance
	// group `[^{;]+?` stops before any `{` or `;`. A trailing `;` with NO brace
	// (forward declaration `class FFoo;`) fails the regex. A match with no opening
	// brace found within the lookahead window is DROPPED (Phase B has no UE-macro
	// anchor proving a definition follows — unlike Phase A).
	FRegexPattern NonUEPattern(TEXT("^\\s*(class|struct)\\s+(?:[A-Z][A-Z0-9]*_API\\s+)?(\\w+)(?:\\s*(?:final|sealed))?\\s*(?::\\s*([^{;]+?))?\\s*(\\{|$)"));

	for (int32 i = 0; i < NumLines; ++i)
	{
		FRegexMatcher Matcher(NonUEPattern, CleanLines[i]);
		if (!Matcher.FindNext())
		{
			continue;
		}

		FString Kind = Matcher.GetCaptureGroup(1);
		FString Name = Matcher.GetCaptureGroup(2);
		if (FoundNames.Contains(Name))
		{
			continue;
		}

		// Template-parameter guard: a multi-line template parameter list can place
		// `class T` alone on a line (matching this pattern as a bogus class) while
		// the real class's `{` sits within the lookahead window below. If the nearest
		// preceding non-empty clean line ends with `<` or `,`, this match is a
		// template parameter, not a declaration — skip it. (The SUBSEQUENT real
		// `class FFoo` line still matches and indexes normally.)
		{
			bool bIsTemplateParam = false;
			for (int32 p = i - 1; p >= 0; --p)
			{
				FString Prev = CleanLines[p].TrimStartAndEnd();
				if (Prev.IsEmpty())
				{
					continue;
				}
				if (Prev.EndsWith(TEXT("<")) || Prev.EndsWith(TEXT(",")))
				{
					bIsTemplateParam = true;
				}
				break; // first non-empty preceding line decides
			}
			if (bIsTemplateParam)
			{
				continue;
			}
		}

		// Confirm the opening brace. It may be on the declaration line itself, or on
		// one of the next few lines (allman). Require the brace to OPEN the line (its
		// first non-whitespace char) — a stricter test than Contains, cheap insurance
		// against an unrelated brace. Mirrors Phase A's k < j+3 lookahead window.
		int32 BraceLineIdx = -1;
		for (int32 k = i; k < FMath::Min(i + 3, NumLines); ++k)
		{
			FString TrimK = CleanLines[k].TrimStart();
			if (k == i && CleanLines[k].Contains(TEXT("{")))
			{
				// Same-line brace (today's K&R style) — accept directly.
				BraceLineIdx = k;
				break;
			}
			if (k > i)
			{
				if (TrimK.IsEmpty())
				{
					continue; // skip blank lines between decl and brace
				}
				if (TrimK.StartsWith(TEXT("{")))
				{
					BraceLineIdx = k;
				}
				break; // first non-blank line after the decl decides
			}
		}

		// No brace found within the window — forward declaration or non-definition.
		// Unlike Phase A there is no UE-macro anchor proving a body follows, so DROP
		// the match (no body, no symbol).
		if (BraceLineIdx < 0)
		{
			continue;
		}

		FParsedSourceSymbol Sym;
		Sym.Kind = Kind;
		Sym.Name = Name;
		Sym.LineStart = i + 1;

		// Base classes
		FString InheritClause = Matcher.GetCaptureGroup(3);
		if (!InheritClause.IsEmpty())
		{
			ParseBaseClasses(InheritClause, Sym.BaseClasses);
		}

		Sym.Docstring = ExtractDocstring(OrigLines, i, false);
		Sym.Signature = OrigLines[i].TrimStartAndEnd();

		// Find closing brace from the confirmed opening-brace line. FindClosingBrace
		// returns Lines.Num() (a sentinel COUNT, NOT a valid index) when the body is
		// unterminated — e.g. an allman `{` at/near EOF with no matching `}`. Treat that
		// as a no-body declaration: record the class row (best-effort LineEnd) but SKIP
		// ExtractMembers, which otherwise receives a body range running past the array.
		const int32 CloseLine = FindClosingBrace(CleanLines, BraceLineIdx);
		const bool bUnterminated = (CloseLine >= CleanLines.Num());

		Sym.LineEnd = bUnterminated ? (i + 1) : CloseLine;

		FString DefaultAccess = Kind == TEXT("struct") ? TEXT("public") : TEXT("private");

		FoundNames.Add(Name);
		Result.Symbols.Add(MoveTemp(Sym));

		if (!bUnterminated)
		{
			// Members live inside the body — scanning must start on the line AFTER the
			// opening brace. BraceLineIdx is a 0-based line index; ExtractMembers treats
			// BodyStartLine as 1-based, so the first body line is BraceLineIdx + 2. Passing
			// BraceLineIdx + 1 would start the scan ON the `{` line, whose brace pushes the
			// member-extractor's BraceDepth to 1 and (since the matching `}` sits past
			// BodyEndLine = CloseLine - 1) the depth never returns to 0 — silently skipping
			// every member, the symptom for allman-style bodies whose `{` is its own line.
			//
			// COPY the parent name first — passing Result.Symbols.Last().Name by reference
			// is a use-after-free: ExtractMembers Adds member symbols to Result.Symbols, and
			// a reallocation frees the backing store the reference points into (AV when the
			// next member reads ParentClass).
			const FString ParentName = Result.Symbols.Last().Name;
			ExtractMembers(OrigLines, BraceLineIdx + 2, CloseLine - 1,
				ParentName, DefaultAccess, Result);
		}
	}
}

// ============================================================
// ExtractEnums
// ============================================================

void FMonolithCppParser::ExtractEnums(const TArray<FString>& OrigLines, const TArray<FString>& CleanLines, FParsedFileResult& Result)
{
	const int32 NumLines = CleanLines.Num();
	TSet<FString> FoundNames;

	FRegexPattern EnumPattern(TEXT("\\benum\\s+(?:class\\s+)?(\\w+)"));

	// Phase A: UENUM-decorated
	for (int32 i = 0; i < OrigLines.Num(); ++i)
	{
		FString Trimmed = OrigLines[i].TrimStartAndEnd();
		if (!Trimmed.StartsWith(TEXT("UENUM(")))
		{
			continue;
		}

		for (int32 j = i; j < FMath::Min(i + 5, NumLines); ++j)
		{
			FRegexMatcher Matcher(EnumPattern, CleanLines[j]);
			if (!Matcher.FindNext())
			{
				continue;
			}

			FParsedSourceSymbol Sym;
			Sym.Kind = TEXT("enum");
			Sym.Name = Matcher.GetCaptureGroup(1);
			Sym.bIsUEMacro = true;
			Sym.LineStart = j + 1;
			Sym.Docstring = ExtractDocstring(OrigLines, i, true);
			Sym.Signature = OrigLines[j].TrimStartAndEnd();

			// Find body
			int32 BraceLineIdx = -1;
			for (int32 k = j; k < FMath::Min(j + 3, NumLines); ++k)
			{
				if (CleanLines[k].Contains(TEXT("{")))
				{
					BraceLineIdx = k;
					break;
				}
			}

			if (BraceLineIdx >= 0)
			{
				Sym.LineEnd = FindClosingBrace(CleanLines, BraceLineIdx);
			}
			else
			{
				Sym.LineEnd = j + 1;
			}

			FoundNames.Add(Sym.Name);
			Result.Symbols.Add(MoveTemp(Sym));
			break;
		}
	}

	// Phase B: Non-UE enums
	for (int32 i = 0; i < NumLines; ++i)
	{
		if (!CleanLines[i].Contains(TEXT("enum")))
		{
			continue;
		}

		FRegexMatcher Matcher(EnumPattern, CleanLines[i]);
		if (!Matcher.FindNext())
		{
			continue;
		}

		FString Name = Matcher.GetCaptureGroup(1);
		if (FoundNames.Contains(Name))
		{
			continue;
		}

		// Skip if this line doesn't have a brace (probably a forward decl or usage)
		bool bHasBrace = false;
		for (int32 k = i; k < FMath::Min(i + 3, NumLines); ++k)
		{
			if (CleanLines[k].Contains(TEXT("{")))
			{
				bHasBrace = true;
				break;
			}
		}
		if (!bHasBrace)
		{
			continue;
		}

		FParsedSourceSymbol Sym;
		Sym.Kind = TEXT("enum");
		Sym.Name = Name;
		Sym.LineStart = i + 1;
		Sym.Docstring = ExtractDocstring(OrigLines, i, false);
		Sym.Signature = OrigLines[i].TrimStartAndEnd();

		// Find body
		int32 BraceLineIdx = -1;
		for (int32 k = i; k < FMath::Min(i + 3, NumLines); ++k)
		{
			if (CleanLines[k].Contains(TEXT("{")))
			{
				BraceLineIdx = k;
				break;
			}
		}

		Sym.LineEnd = (BraceLineIdx >= 0) ? FindClosingBrace(CleanLines, BraceLineIdx) : i + 1;
		FoundNames.Add(Name);
		Result.Symbols.Add(MoveTemp(Sym));
	}
}

// ============================================================
// ExtractFreeFunctions
// ============================================================

void FMonolithCppParser::ExtractFreeFunctions(const TArray<FString>& OrigLines, const TArray<FString>& CleanLines, FParsedFileResult& Result)
{
	using namespace MonolithCppParserDetail;

	const int32 NumLines = CleanLines.Num();

	// Class/struct line ranges (1-based, inclusive) to skip — member functions are
	// extracted by ExtractMembers. Collected once: this pass only appends functions.
	TArray<TPair<int32, int32>> ClassRanges;
	for (const FParsedSourceSymbol& Sym : Result.Symbols)
	{
		if (Sym.Kind == TEXT("class") || Sym.Kind == TEXT("struct"))
		{
			ClassRanges.Emplace(Sym.LineStart, Sym.LineEnd);
		}
	}

	// Definition head: [specifiers] return-type name(   — name may be qualified (A::B).
	// Only the head is matched here; the parameter list is walked with balanced-paren
	// scanning so it may span lines and contain nested parens (TFunction<void()>).
	FRegexPattern HeadPattern(TEXT("^\\s*(?:(?:static|inline|virtual|FORCEINLINE|FORCENOINLINE|constexpr|explicit)\\s+)*(\\w[\\w:*&<>\\s,]*?)\\s+(\\w+(?:::\\w+)*)\\s*\\("));
	// Out-of-line constructor / destructor head (no return type): A::A(  /  A::~A(
	FRegexPattern CtorPattern(TEXT("^\\s*((?:\\w+::)+~?\\w+)\\s*\\("));

	static const TSet<FString> NonFunctionNames = {
		TEXT("if"), TEXT("for"), TEXT("while"), TEXT("switch"), TEXT("catch"), TEXT("else"),
		TEXT("return"), TEXT("sizeof"), TEXT("alignof"), TEXT("decltype"), TEXT("static_assert"),
		TEXT("new"), TEXT("delete"), TEXT("throw"),
	};
	static const TSet<FString> NonReturnTypeWords = {
		TEXT("return"), TEXT("else"), TEXT("new"), TEXT("delete"), TEXT("throw"), TEXT("case"),
		TEXT("goto"), TEXT("typedef"), TEXT("using"), TEXT("co_return"), TEXT("co_yield"), TEXT("co_await"),
	};

	for (int32 i = 0; i < NumLines; ++i)
	{
		// Skip lines inside known class bodies
		bool bInsideClass = false;
		for (const TPair<int32, int32>& Range : ClassRanges)
		{
			if ((i + 1) >= Range.Key && (i + 1) <= Range.Value)
			{
				bInsideClass = true;
				break;
			}
		}
		if (bInsideClass)
		{
			continue;
		}

		const FString& Clean = CleanLines[i];
		if (!Clean.Contains(TEXT("(")))
		{
			continue; // cheap reject before running the regexes
		}

		FString ReturnType;
		FString FuncName;
		int32 ParenCol = INDEX_NONE;
		{
			FRegexMatcher Matcher(HeadPattern, Clean);
			if (Matcher.FindNext())
			{
				ReturnType = Matcher.GetCaptureGroup(1).TrimStartAndEnd();
				FuncName = Matcher.GetCaptureGroup(2);
				ParenCol = Matcher.GetMatchEnding() - 1; // the pattern ends on '('
			}
		}
		if (ParenCol == INDEX_NONE)
		{
			FRegexMatcher Matcher(CtorPattern, Clean);
			if (Matcher.FindNext() && IsQualifiedCtorOrDtor(Matcher.GetCaptureGroup(1)))
			{
				FuncName = Matcher.GetCaptureGroup(1);
				ParenCol = Matcher.GetMatchEnding() - 1;
			}
		}
		if (ParenCol == INDEX_NONE || !Clean.IsValidIndex(ParenCol) || Clean[ParenCol] != TEXT('('))
		{
			continue;
		}

		// Skip things that look like control flow, statements or macros
		if (NonFunctionNames.Contains(FuncName))
		{
			continue;
		}
		{
			int32 WordEnd = 0;
			while (WordEnd < ReturnType.Len() && (FChar::IsAlnum(ReturnType[WordEnd]) || ReturnType[WordEnd] == TEXT('_')))
			{
				++WordEnd;
			}
			if (NonReturnTypeWords.Contains(ReturnType.Left(WordEnd)))
			{
				continue;
			}
		}

		const bool bIsCtor = IsQualifiedCtorOrDtor(FuncName);

		// Walk the parameter list to its closing ')'.
		int32 ParamsEndLine = INDEX_NONE;
		int32 ParamsEndCol = INDEX_NONE;
		if (!FindMatchingParen(CleanLines, i, ParenCol, /*MaxLines=*/15, ParamsEndLine, ParamsEndCol))
		{
			continue;
		}

		// Locate the body's opening brace. Between ')' and '{' only trailing qualifiers
		// may appear (or, for a constructor, an initializer list). Epic's allman style puts
		// the '{' on the next line; K&R puts it on the same line. A ';' first means a
		// prototype or a statement — not a definition.
		int32 BraceLine = INDEX_NONE;
		bool bRejected = false;
		FString Tail;
		for (int32 L = ParamsEndLine; L < FMath::Min(NumLines, ParamsEndLine + 4); ++L)
		{
			const FString Text = StripLineComment(L == ParamsEndLine ? CleanLines[L].Mid(ParamsEndCol) : CleanLines[L]);
			int32 BraceIdx = INDEX_NONE;
			int32 SemiIdx = INDEX_NONE;
			Text.FindChar(TEXT('{'), BraceIdx);
			Text.FindChar(TEXT(';'), SemiIdx);

			if (SemiIdx != INDEX_NONE && (BraceIdx == INDEX_NONE || SemiIdx < BraceIdx))
			{
				bRejected = true;
				break;
			}

			const FString Before = (BraceIdx != INDEX_NONE) ? Text.Left(BraceIdx) : Text;
			Tail += TEXT(" ") + Before;

			// Constructor initializer list: `: Super(X)` (possibly over several lines)
			// precedes the body. Find the body as the first line that opens with '{' or
			// ends with '{'.
			if (bIsCtor && Tail.TrimStart().StartsWith(TEXT(":")))
			{
				for (int32 K = L; K < FMath::Min(NumLines, L + 40); ++K)
				{
					const FString KText = StripLineComment(K == ParamsEndLine ? CleanLines[K].Mid(ParamsEndCol) : CleanLines[K]).TrimStartAndEnd();
					if (K > L && KText.Contains(TEXT(";")))
					{
						bRejected = true;
						break;
					}
					if (KText.StartsWith(TEXT("{")) || KText.EndsWith(TEXT("{")))
					{
						BraceLine = K;
						break;
					}
				}
				if (BraceLine == INDEX_NONE)
				{
					bRejected = true;
				}
				break;
			}

			if (BraceIdx != INDEX_NONE)
			{
				if (IsQualifierTail(Tail))
				{
					BraceLine = L;
				}
				else
				{
					bRejected = true;
				}
				break;
			}
		}
		if (bRejected || BraceLine == INDEX_NONE)
		{
			continue;
		}

		FParsedSourceSymbol Sym;
		Sym.Kind = TEXT("function");
		Sym.LineStart = i + 1;
		Sym.Signature = OrigLines[i].TrimStartAndEnd();
		Sym.Docstring = ExtractDocstring(OrigLines, i, false);

		// If name contains ::, extract parent class
		int32 ScopeIdx;
		if (FuncName.FindLastChar(TEXT(':'), ScopeIdx) && ScopeIdx > 0 && FuncName[ScopeIdx - 1] == TEXT(':'))
		{
			FString QualifiedParent = FuncName.Left(ScopeIdx - 1);
			// Take the last component as parent class
			int32 LastScope;
			if (QualifiedParent.FindLastChar(TEXT(':'), LastScope))
			{
				Sym.ParentClass = QualifiedParent.Mid(LastScope + 1);
			}
			else
			{
				Sym.ParentClass = QualifiedParent;
			}
			// Strip qualification from name
			Sym.Name = FuncName.Mid(ScopeIdx + 1);
		}
		else
		{
			Sym.Name = FuncName;
		}

		// Find closing brace (1-based; NumLines when the body is unterminated)
		const int32 BodyEnd = FindClosingBrace(CleanLines, BraceLine);
		Sym.LineEnd = BodyEnd;

		Result.Symbols.Add(MoveTemp(Sym));

		// Skip the body: statements inside it are never definitions, and scanning them
		// is where false positives would come from. An unterminated body (brace mismatch,
		// e.g. a definition head duplicated across #if branches) does NOT skip, so one bad
		// brace count cannot hide the rest of the file.
		if (BodyEnd < NumLines)
		{
			i = FMath::Max(i, BodyEnd - 1);
		}
	}
}

// ============================================================
// ExtractMacrosAndTypedefs
// ============================================================

void FMonolithCppParser::ExtractMacrosAndTypedefs(const TArray<FString>& OrigLines, FParsedFileResult& Result)
{
	// Skip UE-specific macros
	static const TSet<FString> SkipMacros = {
		TEXT("UCLASS"), TEXT("USTRUCT"), TEXT("UENUM"), TEXT("UINTERFACE"),
		TEXT("UFUNCTION"), TEXT("UPROPERTY"), TEXT("UMETA"),
		TEXT("GENERATED_BODY"), TEXT("GENERATED_UCLASS_BODY"), TEXT("GENERATED_USTRUCT_BODY"),
		TEXT("UE_LOG"), TEXT("UE_DEPRECATED"), TEXT("DECLARE_LOG_CATEGORY_EXTERN"),
		TEXT("DEFINE_LOG_CATEGORY"), TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE"),
		TEXT("DECLARE_DELEGATE"), TEXT("DECLARE_MULTICAST_DELEGATE"),
	};

	FRegexPattern DefinePattern(TEXT("^\\s*#define\\s+(\\w+)"));
	FRegexPattern TypedefPattern(TEXT("^\\s*typedef\\s+.+?\\s+(\\w+)\\s*;"));
	FRegexPattern UsingPattern(TEXT("^\\s*using\\s+(\\w+)\\s*="));

	for (int32 i = 0; i < OrigLines.Num(); ++i)
	{
		const FString& Line = OrigLines[i];

		// #define
		{
			FRegexMatcher Matcher(DefinePattern, Line);
			if (Matcher.FindNext())
			{
				FString MacroName = Matcher.GetCaptureGroup(1);
				if (!SkipMacros.Contains(MacroName) && !MacroName.StartsWith(TEXT("GENERATED_")))
				{
					FParsedSourceSymbol Sym;
					Sym.Kind = TEXT("macro");
					Sym.Name = MacroName;
					Sym.LineStart = i + 1;
					Sym.LineEnd = i + 1;
					Sym.Signature = Line.TrimStartAndEnd();
					Result.Symbols.Add(MoveTemp(Sym));
				}
				continue;
			}
		}

		// typedef
		{
			FRegexMatcher Matcher(TypedefPattern, Line);
			if (Matcher.FindNext())
			{
				FParsedSourceSymbol Sym;
				Sym.Kind = TEXT("typedef");
				Sym.Name = Matcher.GetCaptureGroup(1);
				Sym.LineStart = i + 1;
				Sym.LineEnd = i + 1;
				Sym.Signature = Line.TrimStartAndEnd();
				Result.Symbols.Add(MoveTemp(Sym));
				continue;
			}
		}

		// using alias
		{
			FRegexMatcher Matcher(UsingPattern, Line);
			if (Matcher.FindNext())
			{
				FParsedSourceSymbol Sym;
				Sym.Kind = TEXT("typedef");
				Sym.Name = Matcher.GetCaptureGroup(1);
				Sym.LineStart = i + 1;
				Sym.LineEnd = i + 1;
				Sym.Signature = Line.TrimStartAndEnd();
				Result.Symbols.Add(MoveTemp(Sym));
				continue;
			}
		}
	}
}

// ============================================================
// ExtractMembers — scan inside a class/struct body
// ============================================================

// ParentClass is taken BY VALUE on purpose: this function Adds member symbols to
// Result.Symbols, so any reference into that array's backing store dangles after a
// reallocation. Never pass a reference into Result.Symbols (e.g. Result.Symbols.Last().Name)
// — the by-value copy (one per class, negligible) is the safety net for that mistake.
void FMonolithCppParser::ExtractMembers(const TArray<FString>& OrigLines, int32 BodyStartLine, int32 BodyEndLine,
	FString ParentClass, const FString& DefaultAccess, FParsedFileResult& Result)
{
	// BodyStartLine/BodyEndLine are 1-based line numbers
	// Convert to 0-based indices for array access. BOTH ends must be clamped to the
	// array: a class whose `{` sits at/near EOF with no closing `}` makes
	// FindClosingBrace return Lines.Num() (a sentinel COUNT, not a valid index), which
	// can drive StartIdx past the last element. Without an upper clamp on StartIdx the
	// loop below would read OrigLines[i] out of bounds -> access violation (observed on
	// the background indexer thread against an unterminated real-project header).
	const int32 LastIdx = OrigLines.Num() - 1;
	const int32 StartIdx = FMath::Clamp(BodyStartLine - 1, 0, LastIdx);
	const int32 EndIdx = FMath::Clamp(BodyEndLine - 1, 0, LastIdx);

	// Empty or inverted range (degenerate / unterminated body) -> nothing to extract.
	if (StartIdx > EndIdx)
	{
		return;
	}

	FString CurrentAccess = DefaultAccess;
	bool bNextIsUEMacro = false;
	int32 BraceDepth = 0;

	FRegexPattern AccessPattern(TEXT("^\\s*(public|protected|private)\\s*:"));
	FRegexPattern FuncDeclPattern(TEXT("^\\s*(?:virtual\\s+|static\\s+|inline\\s+|FORCEINLINE\\s+|explicit\\s+|constexpr\\s+)*(\\w[\\w:*&<>,\\s]*?)\\s+(\\w+)\\s*\\([^)]*\\)"));
	FRegexPattern VarDeclPattern(TEXT("^\\s*(\\w[\\w:*&<>,\\s]*?)\\s+(\\w+)\\s*(?:=\\s*[^;]+)?\\s*;"));
	// Head of a declaration whose parameter list continues on the following lines
	// (`UE_API void Foo(int32 A,` / `    int32 B);`). Only tried when the line leaves a
	// '(' unclosed, so single-line forms keep going through FuncDeclPattern.
	FRegexPattern FuncHeadPattern(TEXT("^\\s*(?:virtual\\s+|static\\s+|inline\\s+|FORCEINLINE\\s+|explicit\\s+|constexpr\\s+)*(\\w[\\w:*&<>,\\s]*?)\\s+(\\w+)\\s*\\("));

	auto CountBraces = [&BraceDepth](const FString& Text)
	{
		for (TCHAR Ch : Text)
		{
			if (Ch == TEXT('{')) { ++BraceDepth; }
			else if (Ch == TEXT('}')) { --BraceDepth; }
		}
		if (BraceDepth < 0) { BraceDepth = 0; }
	};

	for (int32 i = StartIdx; i <= EndIdx; ++i)
	{
		const FString& Line = OrigLines[i];
		FString Trimmed = Line.TrimStartAndEnd();

		// Track brace depth — skip nested class/struct/enum bodies and inline method
		// bodies. The decision uses the depth BEFORE this line, so a line that OPENS a
		// body (`void Foo() {`) is still examined as a declaration; only lines inside the
		// body (and its closing `}`) are skipped.
		const int32 DepthBefore = BraceDepth;
		CountBraces(Trimmed);
		if (DepthBefore > 0)
		{
			continue; // Inside a nested scope, skip
		}

		// Skip empty lines
		if (Trimmed.IsEmpty())
		{
			continue;
		}

		// GENERATED_BODY — skip
		if (Trimmed.StartsWith(TEXT("GENERATED_")))
		{
			continue;
		}

		// Access specifier
		{
			FRegexMatcher Matcher(AccessPattern, Line);
			if (Matcher.FindNext())
			{
				CurrentAccess = Matcher.GetCaptureGroup(1);
				continue;
			}
		}

		// UE macro (UFUNCTION, UPROPERTY) — flag next declaration
		if (Trimmed.StartsWith(TEXT("UFUNCTION(")) || Trimmed.StartsWith(TEXT("UPROPERTY(")))
		{
			bNextIsUEMacro = true;
			// Multi-line specifier list: consume continuation lines until the parens
			// balance, so the flag survives to the declaration that follows.
			int32 Balance = MonolithCppParserDetail::ParenBalance(Trimmed);
			for (int32 Guard = 0; Balance > 0 && i < EndIdx && Guard < 30; ++Guard)
			{
				++i;
				CountBraces(OrigLines[i]);
				Balance += MonolithCppParserDetail::ParenBalance(OrigLines[i]);
			}
			continue;
		}

		// Skip comment-only lines
		if (Trimmed.StartsWith(TEXT("//")) || Trimmed.StartsWith(TEXT("/*")) || Trimmed.StartsWith(TEXT("*")))
		{
			continue;
		}

		// Skip #if / #endif / #include etc.
		if (Trimmed.StartsWith(TEXT("#")))
		{
			continue;
		}

		// Skip friend declarations
		if (Trimmed.StartsWith(TEXT("friend ")))
		{
			continue;
		}

		// Skip nested class/struct forward declarations or definitions
		if (Trimmed.StartsWith(TEXT("class ")) || Trimmed.StartsWith(TEXT("struct ")) || Trimmed.StartsWith(TEXT("enum ")))
		{
			continue;
		}

		// Type aliases are not functions (`typedef void (*FFn)(int32);`)
		if (Trimmed.StartsWith(TEXT("typedef ")) || Trimmed.StartsWith(TEXT("using ")))
		{
			bNextIsUEMacro = false;
			continue;
		}

		// Function declaration (with semicolon — declaration, not definition)
		{
			const int32 OpenParens = MonolithCppParserDetail::ParenBalance(Trimmed);

			FString FuncName;
			{
				FRegexMatcher Matcher(FuncDeclPattern, Line);
				if (Matcher.FindNext())
				{
					FuncName = Matcher.GetCaptureGroup(2);
				}
			}
			if (FuncName.IsEmpty() && OpenParens > 0)
			{
				FRegexMatcher Matcher(FuncHeadPattern, Line);
				if (Matcher.FindNext())
				{
					FuncName = Matcher.GetCaptureGroup(2);
				}
			}

			// Filter out things that aren't function names
			if (!FuncName.IsEmpty() && FuncName != TEXT("if") && FuncName != TEXT("for") && FuncName != TEXT("while") &&
				FuncName != TEXT("switch") && FuncName != TEXT("return"))
			{
				FParsedSourceSymbol Sym;
				Sym.Kind = TEXT("function");
				Sym.Name = FuncName;
				Sym.ParentClass = ParentClass;
				Sym.Access = CurrentAccess;
				Sym.bIsUEMacro = bNextIsUEMacro;
				Sym.LineStart = i + 1;
				Sym.Docstring = ExtractDocstring(OrigLines, i, bNextIsUEMacro);

				// Multi-line parameter list: fold the continuation lines into the
				// signature and step past them so they are not re-read as members.
				FString Signature = Trimmed;
				int32 Balance = OpenParens;
				for (int32 Guard = 0; Balance > 0 && i < EndIdx && Guard < 20; ++Guard)
				{
					++i;
					const FString Next = OrigLines[i].TrimStartAndEnd();
					CountBraces(Next);
					Balance += MonolithCppParserDetail::ParenBalance(Next);
					if (!Next.IsEmpty())
					{
						Signature += TEXT(" ") + Next;
					}
				}

				// Declarations keep LineEnd == LineStart even when wrapped: the indexer and
				// read_source treat LineEnd > LineStart as "has a body" (a definition).
				Sym.LineEnd = Sym.LineStart;
				Sym.Signature = Signature;
				Result.Symbols.Add(MoveTemp(Sym));
				bNextIsUEMacro = false;
				continue;
			}
		}

		// Variable declaration
		{
			FRegexMatcher Matcher(VarDeclPattern, Line);
			if (Matcher.FindNext())
			{
				FString VarType = Matcher.GetCaptureGroup(1).TrimStartAndEnd();
				FString VarName = Matcher.GetCaptureGroup(2);

				// Filter out access specifiers and keywords that look like types
				if (VarName != TEXT("return") && VarName != TEXT("break") && VarName != TEXT("continue") &&
					VarType != TEXT("return") && VarType != TEXT("using") && VarType != TEXT("typedef"))
				{
					FParsedSourceSymbol Sym;
					Sym.Kind = TEXT("variable");
					Sym.Name = VarName;
					Sym.ParentClass = ParentClass;
					Sym.Access = CurrentAccess;
					Sym.bIsUEMacro = bNextIsUEMacro;
					Sym.LineStart = i + 1;
					Sym.LineEnd = i + 1;
					Sym.Signature = Trimmed;
					Sym.Docstring = ExtractDocstring(OrigLines, i, bNextIsUEMacro);
					Result.Symbols.Add(MoveTemp(Sym));
					bNextIsUEMacro = false;
					continue;
				}
			}
		}

		// If we reach here, the UE macro flag wasn't consumed — reset it
		// (handles multi-line UPROPERTY specifiers or unrecognized declarations)
		bNextIsUEMacro = false;
	}
}

// ============================================================
// ExtractDocstring — gather comment lines above a target line
// ============================================================

FString FMonolithCppParser::ExtractDocstring(const TArray<FString>& Lines, int32 TargetLine, bool bSkipUEMacro)
{
	// TargetLine is 0-based index
	int32 ScanLine = TargetLine - 1;

	// Skip past UE macro lines if requested
	if (bSkipUEMacro)
	{
		while (ScanLine >= 0 && IsUEMacroLine(Lines[ScanLine]))
		{
			--ScanLine;
		}
	}

	// Collect comment lines going upward
	TArray<FString> CommentLines;

	while (ScanLine >= 0)
	{
		FString Trimmed = Lines[ScanLine].TrimStartAndEnd();

		if (Trimmed.StartsWith(TEXT("///")))
		{
			// Strip /// prefix
			FString Content = Trimmed.Mid(3).TrimStart();
			CommentLines.Insert(Content, 0);
			--ScanLine;
		}
		else if (Trimmed.EndsWith(TEXT("*/")))
		{
			// Block comment — scan upward to find /**
			CommentLines.Insert(Trimmed.Replace(TEXT("*/"), TEXT("")).TrimStartAndEnd(), 0);
			--ScanLine;

			while (ScanLine >= 0)
			{
				FString BlockLine = Lines[ScanLine].TrimStartAndEnd();

				// Strip leading * or /**
				FString Content = BlockLine;
				if (Content.StartsWith(TEXT("/**")))
				{
					Content = Content.Mid(3).TrimStart();
					if (!Content.IsEmpty())
					{
						CommentLines.Insert(Content, 0);
					}
					break;
				}
				else if (Content.StartsWith(TEXT("*")))
				{
					Content = Content.Mid(1).TrimStart();
					if (!Content.IsEmpty())
					{
						CommentLines.Insert(Content, 0);
					}
				}
				else
				{
					// Not part of the block comment
					break;
				}
				--ScanLine;
			}
			break;
		}
		else if (Trimmed.IsEmpty())
		{
			// Allow one blank line between comment and declaration
			--ScanLine;

			// But stop if the next line up isn't a comment either
			if (ScanLine >= 0)
			{
				FString NextUp = Lines[ScanLine].TrimStartAndEnd();
				if (!NextUp.StartsWith(TEXT("///")) && !NextUp.EndsWith(TEXT("*/")))
				{
					break;
				}
			}
		}
		else
		{
			break;
		}
	}

	if (CommentLines.Num() == 0)
	{
		return FString();
	}

	return FString::Join(CommentLines, TEXT("\n"));
}

// ============================================================
// FindClosingBrace — track brace depth from a starting line
// ============================================================

int32 FMonolithCppParser::FindClosingBrace(const TArray<FString>& Lines, int32 OpenBraceLine)
{
	// OpenBraceLine is 0-based index
	int32 Depth = 0;

	for (int32 i = OpenBraceLine; i < Lines.Num(); ++i)
	{
		for (TCHAR Ch : Lines[i])
		{
			if (Ch == TEXT('{')) { ++Depth; }
			else if (Ch == TEXT('}'))
			{
				--Depth;
				if (Depth == 0)
				{
					return i + 1; // 1-based
				}
			}
		}
	}

	// If no closing brace found, return last line
	return Lines.Num();
}

// ============================================================
// IsUEMacroLine
// ============================================================

bool FMonolithCppParser::IsUEMacroLine(const FString& Line)
{
	FString Trimmed = Line.TrimStartAndEnd();
	return Trimmed.StartsWith(TEXT("UCLASS("))
		|| Trimmed.StartsWith(TEXT("USTRUCT("))
		|| Trimmed.StartsWith(TEXT("UENUM("))
		|| Trimmed.StartsWith(TEXT("UINTERFACE("))
		|| Trimmed.StartsWith(TEXT("UFUNCTION("))
		|| Trimmed.StartsWith(TEXT("UPROPERTY("));
}

// ============================================================
// ParseBaseClasses — extract base class names from inheritance clause
// ============================================================

void FMonolithCppParser::ParseBaseClasses(const FString& InheritanceClause, TArray<FString>& OutBases)
{
	// Try pattern with access specifiers first: "public FooBase, protected BarBase"
	FRegexPattern WithAccessPattern(TEXT("(?:public|protected|private)\\s+(\\w+)"));
	FRegexMatcher Matcher(WithAccessPattern, InheritanceClause);

	while (Matcher.FindNext())
	{
		OutBases.Add(Matcher.GetCaptureGroup(1));
	}

	// If nothing matched, try bare names (split by comma)
	if (OutBases.Num() == 0)
	{
		TArray<FString> Parts;
		InheritanceClause.ParseIntoArray(Parts, TEXT(","), true);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			// Take the last word (skip template args etc.)
			FRegexPattern BareNamePattern(TEXT("(\\w+)\\s*$"));
			FRegexMatcher BareMatcher(BareNamePattern, Part);
			if (BareMatcher.FindNext())
			{
				OutBases.Add(BareMatcher.GetCaptureGroup(1));
			}
		}
	}
}
