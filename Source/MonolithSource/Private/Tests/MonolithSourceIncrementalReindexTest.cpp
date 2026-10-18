// SPDX-License-Identifier: MIT
// Regression cover for the incremental project reindex.
//
// An incremental (non-clean) project pass used to append to the existing rows
// instead of replacing them: files deleted from disk kept their symbols, and files
// still on disk had their symbols, includes, source chunks and deprecations
// duplicated. InsertFile/InsertModule also resolved an existing row's id through
// sqlite3_last_insert_rowid() after an ignored INSERT, which returns an unrelated
// rowid. The indexer now purges the project's Source/ and Plugins/ trees before
// re-indexing them, and the insert helpers look the row up first.
//
// Tests use disposable DBs and staged corpora under AutomationTransientDir(),
// never the real EngineSource.db.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MonolithSourceDatabase.h"
#include "MonolithSourceIndexer.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SQLiteDatabase.h"

namespace MonolithSourceIncrementalTestDetail
{
	/** Runs a scalar SELECT on the raw handle; -1 when the statement yields no row. */
	static int64 QueryScalar(FMonolithSourceDatabase& DB, const TCHAR* SQL)
	{
		FScopeLock Lock(&DB.GetLock());
		FSQLiteDatabase* Raw = DB.GetRawHandle();
		if (!Raw)
		{
			return -1;
		}
		FSQLitePreparedStatement Stmt;
		Stmt.Create(*Raw, SQL);
		int64 Value = -1;
		if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			Stmt.GetColumnValueByIndex(0, Value);
		}
		return Value;
	}

	static bool WriteLines(const FString& Path, const TArray<FString>& Lines)
	{
		return FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *Path);
	}

	static bool RunIndex(const FString& ProjectDir, const FString& DbPath, bool bClean)
	{
		FMonolithSourceIndexer Indexer;
		Indexer.SetSourcePath(TEXT(""));    // project phase only
		Indexer.SetShaderPath(TEXT(""));
		Indexer.SetProjectPath(ProjectDir);
		Indexer.SetDatabasePath(DbPath);
		Indexer.SetCleanBuild(bClean);
		Indexer.SetIndexProjectSource(true);
		return Indexer.RunSynchronous();
	}
}

// ---------------------------------------------------------------------------
// ProjectPurge — clean index, then delete B.h and drop a class from A.h, then
// an incremental pass. The deleted file and the removed class must be gone,
// and nothing derived from A.h may be duplicated.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMonolithSourceIncrementalProjectPurgeTest,
	"Monolith.Source.IncrementalReindex.ProjectPurge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonolithSourceIncrementalProjectPurgeTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithSourceIncrementalTestDetail;

	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::AutomationTransientDir(),
		TEXT("MonolithSourceIncrementalTests"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	const FString ProjectDir = Root / TEXT("Project");
	const FString ModuleDir = ProjectDir / TEXT("Source") / TEXT("PurgeTestMod");
	const FString DbPath = Root / TEXT("EngineSource.db");
	const FString HeaderA = ModuleDir / TEXT("A.h");
	const FString HeaderB = ModuleDir / TEXT("B.h");

	IFileManager::Get().MakeDirectory(*ModuleDir, /*Tree=*/true);
	ON_SCOPE_EXIT
	{
		IFileManager::Get().DeleteDirectory(*Root, /*bRequireExists=*/false, /*bTree=*/true);
	};

	const TArray<FString> AFirst = {
		TEXT("#pragma once"),
		TEXT("#include \"CoreMinimal.h\""),
		TEXT("class AFoo"),
		TEXT("{"),
		TEXT("public:"),
		TEXT("\tint32 Value;"),
		TEXT("\tUE_DEPRECATED(5.4, \"Use NewThing instead\")"),
		TEXT("\tvoid OldThing();"),
		TEXT("};"),
		TEXT("class FRemovedLater"),
		TEXT("{"),
		TEXT("public:"),
		TEXT("\tint32 Gone;"),
		TEXT("};"),
	};
	const TArray<FString> ASecond = {
		TEXT("#pragma once"),
		TEXT("#include \"CoreMinimal.h\""),
		TEXT("class AFoo"),
		TEXT("{"),
		TEXT("public:"),
		TEXT("\tint32 Value;"),
		TEXT("\tUE_DEPRECATED(5.4, \"Use NewThing instead\")"),
		TEXT("\tvoid OldThing();"),
		TEXT("};"),
	};
	const TArray<FString> BOnly = {
		TEXT("#pragma once"),
		TEXT("#include \"A.h\""),
		TEXT("class BBar : public AFoo"),
		TEXT("{"),
		TEXT("public:"),
		TEXT("\tint32 Other;"),
		TEXT("};"),
	};

	if (!WriteLines(HeaderA, AFirst) || !WriteLines(HeaderB, BOnly))
	{
		AddError(TEXT("Could not stage the corpus"));
		return false;
	}

	// --- Pass 1: clean build ---
	TestTrue(TEXT("clean index ran"), RunIndex(ProjectDir, DbPath, /*bClean=*/true));
	{
		FMonolithSourceDatabase DB;
		if (!DB.Open(DbPath))
		{
			AddError(TEXT("Failed to open the DB after the clean pass"));
			return false;
		}
		// Sanity: the corpus indexed the way the rest of this test assumes.
		TestEqual(TEXT("pass 1: BBar indexed"),
			QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE name = 'BBar' AND kind = 'class';")), int64(1));
		TestEqual(TEXT("pass 1: FRemovedLater indexed"),
			QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE name = 'FRemovedLater';")), int64(1));
		TestEqual(TEXT("pass 1: two files"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM files;")), int64(2));
		TestEqual(TEXT("pass 1: one deprecation"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbol_deprecations;")), int64(1));
		DB.Close();
	}

	// --- Mutate: delete B.h, drop FRemovedLater from A.h ---
	TestTrue(TEXT("B.h deleted"), IFileManager::Get().Delete(*HeaderB, /*bRequireExists=*/true, /*bEvenReadOnly=*/true));
	TestTrue(TEXT("A.h rewritten"), WriteLines(HeaderA, ASecond));

	// --- Pass 2: incremental, project only ---
	TestTrue(TEXT("incremental index ran"), RunIndex(ProjectDir, DbPath, /*bClean=*/false));

	FMonolithSourceDatabase DB;
	if (!DB.Open(DbPath))
	{
		AddError(TEXT("Failed to open the DB after the incremental pass"));
		return false;
	}
	ON_SCOPE_EXIT
	{
		DB.Close();
	};

	// Deleted file and removed class are gone, symbols_fts included.
	TestEqual(TEXT("BBar gone"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE name = 'BBar';")), int64(0));
	TestEqual(TEXT("FRemovedLater gone"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE name = 'FRemovedLater';")), int64(0));
	TestEqual(TEXT("Gone member gone"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE name = 'Gone';")), int64(0));
	TestEqual(TEXT("BBar absent from symbols_fts"), DB.SearchSymbolsFTS(TEXT("BBar")).Num(), 0);
	TestEqual(TEXT("B.h file row gone"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM files WHERE replace(path, '\\', '/') LIKE '%/B.h';")), int64(0));
	TestEqual(TEXT("no inheritance rows left for BBar"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM inheritance;")), int64(0));

	// A.h survives exactly once, with nothing duplicated.
	TestEqual(TEXT("AFoo exactly once"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE name = 'AFoo' AND kind = 'class';")), int64(1));
	TestEqual(TEXT("AFoo::Value exactly once"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE qualified_name = 'AFoo::Value';")), int64(1));
	TestEqual(TEXT("one file row"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM files;")), int64(1));
	TestEqual(TEXT("one module row"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM modules WHERE name = 'PurgeTestMod';")), int64(1));

	const int64 AFileId = QueryScalar(DB, TEXT("SELECT id FROM files WHERE replace(path, '\\', '/') LIKE '%/A.h';"));
	TestTrue(TEXT("A.h file row present"), AFileId > 0);

	TestEqual(TEXT("one include (A.h's CoreMinimal.h)"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM includes;")), int64(1));
	TestEqual(TEXT("include belongs to A.h"),
		QueryScalar(DB, *FString::Printf(TEXT("SELECT COUNT(*) FROM includes WHERE file_id = %lld;"), AFileId)), int64(1));

	// source_fts: 9 lines in A.h -> exactly one 10-line chunk, and no orphans.
	TestEqual(TEXT("one source chunk"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM source_fts;")), int64(1));
	TestEqual(TEXT("source chunk belongs to A.h"),
		QueryScalar(DB, *FString::Printf(TEXT("SELECT COUNT(*) FROM source_fts WHERE file_id = %lld;"), AFileId)), int64(1));

	// Deprecations: exactly one row, linked to A.h (schema v3 file_id).
	TestEqual(TEXT("one deprecation"), QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbol_deprecations;")), int64(1));
	TestEqual(TEXT("deprecation carries A.h's file_id"),
		QueryScalar(DB, *FString::Printf(TEXT("SELECT COUNT(*) FROM symbol_deprecations WHERE file_id = %lld AND symbol_name = 'OldThing';"), AFileId)),
		int64(1));

	// No row anywhere may point at a file or symbol that no longer exists.
	TestEqual(TEXT("no orphan symbols"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE file_id NOT IN (SELECT id FROM files);")), int64(0));
	TestEqual(TEXT("no dangling parent_symbol_id"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbols WHERE parent_symbol_id IS NOT NULL AND parent_symbol_id NOT IN (SELECT id FROM symbols);")), int64(0));
	TestEqual(TEXT("no orphan references"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM \"references\" WHERE file_id NOT IN (SELECT id FROM files) OR (from_symbol_id <> 0 AND from_symbol_id NOT IN (SELECT id FROM symbols)) OR to_symbol_id NOT IN (SELECT id FROM symbols);")), int64(0));

	return true;
}

// ---------------------------------------------------------------------------
// InsertIdResolution — InsertModule/InsertFile on an existing row return that
// row's id (not the last rowid inserted on the connection) and refresh its
// metadata.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMonolithSourceInsertIdResolutionTest,
	"Monolith.Source.IncrementalReindex.InsertIdResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonolithSourceInsertIdResolutionTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithSourceIncrementalTestDetail;

	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::AutomationTransientDir(),
		TEXT("MonolithSourceIncrementalTests"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	IFileManager::Get().MakeDirectory(*Root, /*Tree=*/true);
	const FString DbPath = Root / TEXT("EngineSource.db");

	FMonolithSourceDatabase DB;
	ON_SCOPE_EXIT
	{
		DB.Close();
		IFileManager::Get().DeleteDirectory(*Root, /*bRequireExists=*/false, /*bTree=*/true);
	};

	if (!DB.OpenForWriting(DbPath) || !DB.CreateTablesIfNeeded())
	{
		AddError(TEXT("Could not create the test DB"));
		return false;
	}

	const int64 ModuleId = DB.InsertModule(TEXT("ModA"), TEXT("C:/P/Source/ModA"), TEXT("Project"));
	const int64 FileId = DB.InsertFile(TEXT("C:/P/Source/ModA/X.h"), ModuleId, TEXT("header"), 10, 1.0);

	// Advance last_insert_rowid well past both ids with unrelated inserts.
	int64 LastSymbolId = 0;
	for (int32 i = 0; i < 5; ++i)
	{
		LastSymbolId = DB.InsertSymbol(FString::Printf(TEXT("S%d"), i), FString::Printf(TEXT("S%d"), i), TEXT("function"),
			FileId, 1, 1, 0, TEXT(""), TEXT(""), TEXT(""), false);
	}
	TestTrue(TEXT("symbol ids diverged from the file id"), LastSymbolId != FileId);

	TestEqual(TEXT("existing module resolves to its own id"),
		DB.InsertModule(TEXT("ModA"), TEXT("C:/P/Source/ModA"), TEXT("Project"), TEXT("C:/P/Source/ModA/ModA.Build.cs")), ModuleId);
	TestEqual(TEXT("existing file resolves to its own id"),
		DB.InsertFile(TEXT("C:/P/Source/ModA/X.h"), ModuleId, TEXT("header"), 42, 2.0), FileId);

	TestEqual(TEXT("file line_count refreshed"),
		QueryScalar(DB, *FString::Printf(TEXT("SELECT line_count FROM files WHERE id = %lld;"), FileId)), int64(42));
	TestEqual(TEXT("module build_cs_path refreshed"),
		QueryScalar(DB, *FString::Printf(TEXT("SELECT COUNT(*) FROM modules WHERE id = %lld AND build_cs_path = 'C:/P/Source/ModA/ModA.Build.cs';"), ModuleId)),
		int64(1));

	// A new row still gets a fresh id.
	const int64 SecondFile = DB.InsertFile(TEXT("C:/P/Source/ModA/Y.h"), ModuleId, TEXT("header"), 3, 1.0);
	TestTrue(TEXT("new file gets a new id"), SecondFile > 0 && SecondFile != FileId);
	TestEqual(TEXT("new file id is its own"),
		QueryScalar(DB, TEXT("SELECT id FROM files WHERE path = 'C:/P/Source/ModA/Y.h';")), SecondFile);

	return true;
}

// ---------------------------------------------------------------------------
// DeprecationFileIdMigration — a pre-v3 symbol_deprecations table (no file_id)
// gains the column in place, keeps its rows, and stamps schema v3.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMonolithSourceDeprecationFileIdMigrationTest,
	"Monolith.Source.IncrementalReindex.DeprecationFileIdMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonolithSourceDeprecationFileIdMigrationTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithSourceIncrementalTestDetail;

	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::AutomationTransientDir(),
		TEXT("MonolithSourceIncrementalTests"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	IFileManager::Get().MakeDirectory(*Root, /*Tree=*/true);
	const FString DbPath = Root / TEXT("EngineSource.db");

	FMonolithSourceDatabase DB;
	ON_SCOPE_EXIT
	{
		DB.Close();
		IFileManager::Get().DeleteDirectory(*Root, /*bRequireExists=*/false, /*bTree=*/true);
	};

	if (!DB.OpenForWriting(DbPath))
	{
		AddError(TEXT("Could not create the test DB"));
		return false;
	}

	// Recreate the v2 table shape and a legacy row before the schema pass runs.
	{
		FScopeLock Lock(&DB.GetLock());
		FSQLiteDatabase* Raw = DB.GetRawHandle();
		TestNotNull(TEXT("raw handle"), Raw);
		if (!Raw)
		{
			return false;
		}
		TestTrue(TEXT("v2 table created"), Raw->Execute(
			TEXT("CREATE TABLE symbol_deprecations (id INTEGER PRIMARY KEY AUTOINCREMENT, symbol_id INTEGER REFERENCES symbols(id), ")
			TEXT("symbol_name TEXT NOT NULL, version TEXT, message TEXT, kind TEXT NOT NULL);")));
		TestTrue(TEXT("legacy row inserted"), Raw->Execute(
			TEXT("INSERT INTO symbol_deprecations (symbol_name, version, message, kind) VALUES ('Legacy', '5.0', 'old', 'UE_DEPRECATED');")));
	}

	TestTrue(TEXT("CreateTablesIfNeeded migrates"), DB.CreateTablesIfNeeded());
	TestEqual(TEXT("schema_version stamped to 3"), DB.GetMeta(TEXT("schema_version")), FString(TEXT("3")));
	TestEqual(TEXT("file_id column added"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM pragma_table_info('symbol_deprecations') WHERE name = 'file_id';")), int64(1));
	TestEqual(TEXT("legacy row kept with NULL file_id"),
		QueryScalar(DB, TEXT("SELECT COUNT(*) FROM symbol_deprecations WHERE symbol_name = 'Legacy' AND file_id IS NULL;")), int64(1));

	// Running the schema pass again is a no-op.
	TestTrue(TEXT("second CreateTablesIfNeeded"), DB.CreateTablesIfNeeded());

	DB.InsertDeprecation(0, TEXT("Fresh"), TEXT("5.7"), TEXT("new"), TEXT("UE_DEPRECATED"), /*FileId=*/7);
	TestEqual(TEXT("file_id stored"),
		QueryScalar(DB, TEXT("SELECT file_id FROM symbol_deprecations WHERE symbol_name = 'Fresh';")), int64(7));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
