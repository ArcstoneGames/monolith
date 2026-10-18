// SPDX-License-Identifier: MIT
// Parser regression tests for issue #156: member functions of UCLASS/USTRUCT types
// and allman-style out-of-line definitions must produce `function` symbols.
//
// The fixture sources are written to AutomationTransientDir at runtime (not checked
// in as headers) so UnrealHeaderTool never sees the UCLASS declarations.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MonolithCppParser.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace MonolithCppParserMembersTestDetail
{
	static FString WriteTempSource(const FString& Extension, const TArray<FString>& Lines)
	{
		const FString Dir = FPaths::AutomationTransientDir() / TEXT("CppParserMembers");
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
		const FString Path = Dir / FString::Printf(TEXT("%s.%s"), *FGuid::NewGuid().ToString(), *Extension);
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\r\n")), *Path);
		return Path;
	}

	static bool HasFunction(const FParsedFileResult& Result, const FString& Parent, const FString& Name)
	{
		for (const FParsedSourceSymbol& Sym : Result.Symbols)
		{
			if (Sym.Kind == TEXT("function") && Sym.Name == Name && Sym.ParentClass == Parent)
			{
				return true;
			}
		}
		return false;
	}

	static int32 CountFunctionsNamed(const FParsedFileResult& Result, const FString& Name)
	{
		int32 Count = 0;
		for (const FParsedSourceSymbol& Sym : Result.Symbols)
		{
			if (Sym.Kind == TEXT("function") && Sym.Name == Name)
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMonolithCppParserUClassMembersTest,
	"Monolith.Source.CppParser.UClassMembers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonolithCppParserUClassMembersTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithCppParserMembersTestDetail;

	// Mirrors CommonUserWidget.h: `#define UE_API X_API`, UCLASS with nested parens,
	// allman brace, GENERATED_UCLASS_BODY, UE_API-prefixed methods, inline bodies.
	const FString Header = WriteTempSource(TEXT("h"), {
		TEXT("#pragma once"),
		TEXT("#define UE_API TESTMOD_API"),
		TEXT("UCLASS(MinimalAPI, meta = (Category = \"Test\", DisableNativeTick))"),
		TEXT("class UParserTestWidget : public UUserWidget"),
		TEXT("{"),
		TEXT("\tGENERATED_UCLASS_BODY()"),
		TEXT("public:"),
		TEXT("\tUFUNCTION(BlueprintCallable, Category = Test)"),
		TEXT("\tUE_API void SetConsumePointerInput(bool bIn);"),
		TEXT("\tUFUNCTION(BlueprintCallable,"),
		TEXT("\t\tCategory = Test)"),
		TEXT("\tUE_API void MultiLineSpecifier();"),
		TEXT("\tconst TArray<int32>& GetBindings() const { return Bindings; }"),
		TEXT("\tUE_API virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,"),
		TEXT("\t\tconst FPointerEvent& InMouseEvent) override;"),
		TEXT("\tint32 KAndRInline() const {"),
		TEXT("\t\treturn 1;"),
		TEXT("\t}"),
		TEXT("\ttypedef void (*FCallback)(int32);"),
		TEXT("protected:"),
		TEXT("\tUPROPERTY(EditAnywhere)"),
		TEXT("\tbool bFlag = false;"),
		TEXT("private:"),
		TEXT("\tTArray<int32> Bindings;"),
		TEXT("};"),
		TEXT("#undef UE_API"),
	});

	FMonolithCppParser Parser;
	const FParsedFileResult H = Parser.ParseFile(Header);
	TestTrue(TEXT("UCLASS method extracted"), HasFunction(H, TEXT("UParserTestWidget"), TEXT("SetConsumePointerInput")));
	TestTrue(TEXT("method after multi-line UFUNCTION extracted"), HasFunction(H, TEXT("UParserTestWidget"), TEXT("MultiLineSpecifier")));
	TestTrue(TEXT("inline-body method extracted"), HasFunction(H, TEXT("UParserTestWidget"), TEXT("GetBindings")));
	TestTrue(TEXT("multi-line declaration extracted"), HasFunction(H, TEXT("UParserTestWidget"), TEXT("NativeOnMouseButtonDown")));
	TestTrue(TEXT("K&R inline method extracted"), HasFunction(H, TEXT("UParserTestWidget"), TEXT("KAndRInline")));
	TestEqual(TEXT("typedef is not a function"), CountFunctionsNamed(H, TEXT("void")), 0);
	TestEqual(TEXT("continuation line is not a member"), CountFunctionsNamed(H, TEXT("InMouseEvent")), 0);

	// Mirrors CommonUserWidget.cpp: allman definitions and a constructor with an
	// initializer list. Previously no symbols at all were extracted from this shape.
	const FString Source = WriteTempSource(TEXT("cpp"), {
		TEXT("#include \"ParserTestWidget.h\""),
		TEXT("UParserTestWidget::UParserTestWidget(const FObjectInitializer& ObjectInitializer)"),
		TEXT("\t: Super(ObjectInitializer)"),
		TEXT("{"),
		TEXT("}"),
		TEXT(""),
		TEXT("void UParserTestWidget::SetConsumePointerInput(bool bIn)"),
		TEXT("{"),
		TEXT("\tif (bIn)"),
		TEXT("\t{"),
		TEXT("\t\tHelper(bIn);"),
		TEXT("\t}"),
		TEXT("}"),
		TEXT(""),
		TEXT("FReply UParserTestWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,"),
		TEXT("\tconst FPointerEvent& InMouseEvent)"),
		TEXT("{"),
		TEXT("\treturn FReply::Handled();"),
		TEXT("}"),
		TEXT(""),
		TEXT("static TAutoConsoleVariable<int32> CVarParserTest("),
		TEXT("\tTEXT(\"p.Test\"), 0, TEXT(\"help\"));"),
		TEXT(""),
		TEXT("int32 UParserTestWidget::KAndRInline() const {"),
		TEXT("\treturn 1;"),
		TEXT("}"),
	});

	const FParsedFileResult C = Parser.ParseFile(Source);
	TestTrue(TEXT("constructor with initializer list extracted"), HasFunction(C, TEXT("UParserTestWidget"), TEXT("UParserTestWidget")));
	TestTrue(TEXT("allman definition extracted"), HasFunction(C, TEXT("UParserTestWidget"), TEXT("SetConsumePointerInput")));
	TestTrue(TEXT("multi-line allman definition extracted"), HasFunction(C, TEXT("UParserTestWidget"), TEXT("NativeOnMouseButtonDown")));
	TestTrue(TEXT("K&R definition still extracted"), HasFunction(C, TEXT("UParserTestWidget"), TEXT("KAndRInline")));
	TestEqual(TEXT("call inside a body is not a definition"), CountFunctionsNamed(C, TEXT("Helper")), 0);
	TestEqual(TEXT("console variable is not a function"), CountFunctionsNamed(C, TEXT("CVarParserTest")), 0);

	IFileManager::Get().Delete(*Header, false, true);
	IFileManager::Get().Delete(*Source, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
