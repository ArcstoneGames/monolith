// Copyright tumourlove. All Rights Reserved.

// =============================================================================
// MonolithPiePreflightTests.cpp
//
// Regression tests for the PIE pre-flight scan behind list_errored_blueprints,
// start_pie, run_pie_smoke and sample_pie_timeseries.
//
//   - Monolith.Editor.PIE.PreflightReportsDirtyBlueprintWithCompileErrors
//       A Blueprint whose last compile FAILED and which has since been marked
//       Dirty (BS_Dirty, not BS_Error) is still reported. The engine's
//       ResolveDirtyBlueprints (PlayLevel.cpp) recompiles every Dirty
//       Blueprint before PIE and raises the blocking compile-error modal on
//       any that fail, so a pre-flight that only counts BS_Error passes a
//       world the engine then blocks on.
//
// SCOPE: a transient Blueprint in the transient package, compiled once with
// a deliberate error. Nothing is saved, no PIE session is started, and the
// Blueprint is marked garbage at the end, so the suite is safe in a live editor.
// =============================================================================

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "Components/PrimitiveComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

#include "MonolithEditorActions.h"
#include "MonolithToolRegistry.h" // FMonolithActionResult

namespace MonolithPiePreflightTests
{
	/** True when list_errored_blueprints' result names the Blueprint at BlueprintPath. */
	static bool ResultListsBlueprint(const FMonolithActionResult& Result, const FString& BlueprintPath)
	{
		if (!Result.bSuccess || !Result.Result.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Result.Result->TryGetArrayField(TEXT("blueprints"), Items) || !Items)
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			FString Path;
			if (Item.IsValid() && Item->TryGetObject(Obj) && Obj && (*Obj)->TryGetStringField(TEXT("path"), Path)
				&& Path == BlueprintPath)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMonolithPreflightReportsDirtyBlueprintWithCompileErrorsTest,
	"Monolith.Editor.PIE.PreflightReportsDirtyBlueprintWithCompileErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonolithPreflightReportsDirtyBlueprintWithCompileErrorsTest::RunTest(const FString& /*Parameters*/)
{
	using namespace MonolithPiePreflightTests;

	// Unique per run: CreateBlueprint asserts on a name collision, and an earlier run's
	// garbage-marked Blueprint may not have been collected yet.
	UPackage* Package = GetTransientPackage();
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Package,
		MakeUniqueObjectName(Package, UBlueprint::StaticClass(), TEXT("BP_MonolithPreflightTest")),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("(rig) transient Blueprint created"), Blueprint)
		|| !TestTrue(TEXT("(rig) Blueprint has an event graph"), Blueprint->UbergraphPages.Num() > 0))
	{
		return false;
	}
	const FString BlueprintPath = Blueprint->GetPathName();
	UEdGraph* Graph = Blueprint->UbergraphPages[0];

	// The deliberate error: a custom event wired to UPrimitiveComponent::SetSimulatePhysics
	// with no Target. An Actor Blueprint's self is not a PrimitiveComponent, so the call
	// must have a connected Target and the compile fails. The event is a root node, so the
	// call is not pruned as isolated before validation.
	UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph);
	Event->CustomFunctionName = TEXT("MonolithPreflightTestEvent");
	Graph->AddNode(Event, /*bFromUI=*/false, /*bSelectNewNode=*/false);
	Event->CreateNewGuid();
	Event->PostPlacedNewNode();
	Event->AllocateDefaultPins();

	UFunction* SetSimulatePhysics = UPrimitiveComponent::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UPrimitiveComponent, SetSimulatePhysics));
	if (!TestNotNull(TEXT("(rig) UPrimitiveComponent::SetSimulatePhysics found"), SetSimulatePhysics))
	{
		Blueprint->MarkAsGarbage();
		return false;
	}
	UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
	Call->SetFromFunction(SetSimulatePhysics);
	Graph->AddNode(Call, /*bFromUI=*/false, /*bSelectNewNode=*/false);
	Call->CreateNewGuid();
	Call->PostPlacedNewNode();
	Call->AllocateDefaultPins();

	UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* Exec = Call->GetExecPin();
	if (!TestTrue(TEXT("(rig) event Then and call Exec pins exist"), Then != nullptr && Exec != nullptr))
	{
		Blueprint->MarkAsGarbage();
		return false;
	}
	Then->MakeLinkTo(Exec);

	// The compiler echoes the deliberate error to LogBlueprint at Error verbosity.
	AddExpectedErrorPlain(TEXT("must have a connection"), EAutomationExpectedErrorFlags::Contains, 0);
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	TestEqual(TEXT("(rig) the compile failed: status is BS_Error"), (int32)Blueprint->Status, (int32)BS_Error);

	// Baseline: the case the pre-flight already caught.
	const FMonolithActionResult Errored = FMonolithEditorActions::HandleListErroredBlueprints(MakeShared<FJsonObject>());
	TestTrue(TEXT("BS_Error Blueprint is reported"), ResultListsBlueprint(Errored, BlueprintPath));

	// The missed case: the Blueprint is modified after the failed compile, so it is now
	// BS_Dirty. Nothing recompiled it; the engine's pre-PIE compile will hit the same error.
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	TestEqual(TEXT("(rig) after a modification the status is BS_Dirty"), (int32)Blueprint->Status, (int32)BS_Dirty);

	const FMonolithActionResult Dirty = FMonolithEditorActions::HandleListErroredBlueprints(MakeShared<FJsonObject>());
	TestTrue(TEXT("Dirty Blueprint whose last compile failed is still reported"), ResultListsBlueprint(Dirty, BlueprintPath));

	// The scan is read-only: it did not compile the Blueprint.
	TestEqual(TEXT("The scan left the Blueprint Dirty (no compile)"), (int32)Blueprint->Status, (int32)BS_Dirty);

	// And a Dirty Blueprint with no recorded error is not reported: fix the error, compile
	// clean, then dirty it again.
	Call->BreakAllNodeLinks();
	Graph->RemoveNode(Call);
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	TestTrue(TEXT("(rig) after removing the bad call the Blueprint compiles clean"), Blueprint->IsUpToDate());
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	const FMonolithActionResult Clean = FMonolithEditorActions::HandleListErroredBlueprints(MakeShared<FJsonObject>());
	TestFalse(TEXT("Dirty Blueprint whose last compile succeeded is not reported"), ResultListsBlueprint(Clean, BlueprintPath));

	Blueprint->MarkAsGarbage();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
