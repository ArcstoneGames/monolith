#include "MonolithMeshFloorPlanGenerator.h"
#include "MonolithToolRegistry.h"
#include "MonolithParamSchema.h"
#include "MonolithJsonUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMonolithFloorPlan, Log, All);
DEFINE_LOG_CATEGORY(LogMonolithFloorPlan);

// ============================================================================
// Registration
// ============================================================================

void FMonolithMeshFloorPlanGenerator::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("mesh"), TEXT("generate_floor_plan"),
		TEXT("Generate a complete floor plan from a building archetype. Returns grid, rooms, and doors "
			"in the exact format consumed by create_building_from_grid. "
			"Algorithm: archetype loading -> room resolution -> squarified treemap layout -> corridor insertion -> door placement."),
		FMonolithActionHandler::CreateStatic(&FMonolithMeshFloorPlanGenerator::GenerateFloorPlan),
		FParamSchemaBuilder()
			.Required(TEXT("archetype"), TEXT("string"), TEXT("Archetype name (e.g. 'residential_house') or full path to archetype JSON file"))
			.Required(TEXT("footprint_width"), TEXT("number"), TEXT("Building footprint width in cm (e.g. 800 = 8m)"))
			.Required(TEXT("footprint_height"), TEXT("number"), TEXT("Building footprint height/depth in cm (e.g. 600 = 6m)"))
			.Optional(TEXT("cell_size"), TEXT("number"), TEXT("Grid cell size in cm. Smaller = finer resolution, larger grids"), TEXT("50"))
			.Optional(TEXT("seed"), TEXT("number"), TEXT("Random seed for deterministic generation. -1 = random"), TEXT("-1"))
			.Optional(TEXT("hospice_mode"), TEXT("boolean"), TEXT("Enforce wheelchair accessibility: min 100cm doors, 180cm corridors, rest alcoves"), TEXT("false"))
			.Optional(TEXT("min_room_aspect"), TEXT("number"), TEXT("Minimum acceptable room aspect ratio (width/height). Rooms worse than this get rebalanced"), TEXT("3.0"))
			.Optional(TEXT("floor_index"), TEXT("number"), TEXT("Floor index for per-floor room filtering: 0=ground, 1+=upper, -1=all floors (default)"), TEXT("-1"))
			.Build());

	Registry.RegisterAction(TEXT("mesh"), TEXT("list_building_archetypes"),
		TEXT("List all available building archetype JSON files in the archetypes directory."),
		FMonolithActionHandler::CreateStatic(&FMonolithMeshFloorPlanGenerator::ListBuildingArchetypes),
		FParamSchemaBuilder()
			.Build());

	Registry.RegisterAction(TEXT("mesh"), TEXT("get_building_archetype"),
		TEXT("Return the full JSON definition of a specific building archetype."),
		FMonolithActionHandler::CreateStatic(&FMonolithMeshFloorPlanGenerator::GetBuildingArchetype),
		FParamSchemaBuilder()
			.Required(TEXT("archetype"), TEXT("string"), TEXT("Archetype name (e.g. 'residential_house') without .json extension"))
			.Build());
}

// ============================================================================
// File I/O
// ============================================================================

FString FMonolithMeshFloorPlanGenerator::GetArchetypeDirectory()
{
	return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Monolith"), TEXT("Saved"), TEXT("Monolith"), TEXT("BuildingArchetypes"));
}

bool FMonolithMeshFloorPlanGenerator::LoadArchetype(const FString& ArchetypeName, FBuildingArchetype& OutArchetype, FString& OutError)
{
	FString FilePath;
	if (ArchetypeName.EndsWith(TEXT(".json")))
	{
		FilePath = ArchetypeName;
	}
	else
	{
		FilePath = FPaths::Combine(GetArchetypeDirectory(), ArchetypeName + TEXT(".json"));
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not load archetype file: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> Json = FMonolithJsonUtils::Parse(JsonString);
	if (!Json.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to parse archetype JSON: %s"), *FilePath);
		return false;
	}

	return ParseArchetypeJson(Json, OutArchetype, OutError);
}

bool FMonolithMeshFloorPlanGenerator::ParseArchetypeJson(const TSharedPtr<FJsonObject>& Json, FBuildingArchetype& OutArchetype, FString& OutError)
{
	Json->TryGetStringField(TEXT("name"), OutArchetype.Name);
	Json->TryGetStringField(TEXT("description"), OutArchetype.Description);
	Json->TryGetStringField(TEXT("roof_type"), OutArchetype.RoofType);

	// Parse floor_height
	if (Json->HasField(TEXT("floor_height")))
		OutArchetype.FloorHeight = static_cast<float>(Json->GetNumberField(TEXT("floor_height")));

	// Parse material_hints
	const TSharedPtr<FJsonObject>* MatHintsObj = nullptr;
	if (Json->TryGetObjectField(TEXT("material_hints"), MatHintsObj) && MatHintsObj && (*MatHintsObj).IsValid())
	{
		(*MatHintsObj)->TryGetStringField(TEXT("exterior"), OutArchetype.MaterialHints.Exterior);
		(*MatHintsObj)->TryGetStringField(TEXT("interior"), OutArchetype.MaterialHints.Interior);
		(*MatHintsObj)->TryGetStringField(TEXT("floor"), OutArchetype.MaterialHints.FloorMaterial);
	}

	// Parse floors
	const TSharedPtr<FJsonObject>* FloorsObj = nullptr;
	if (Json->TryGetObjectField(TEXT("floors"), FloorsObj) && FloorsObj && (*FloorsObj).IsValid())
	{
		if ((*FloorsObj)->HasField(TEXT("min")))
			OutArchetype.FloorsMin = static_cast<int32>((*FloorsObj)->GetNumberField(TEXT("min")));
		if ((*FloorsObj)->HasField(TEXT("max")))
			OutArchetype.FloorsMax = static_cast<int32>((*FloorsObj)->GetNumberField(TEXT("max")));
	}

	// Parse rooms
	const TArray<TSharedPtr<FJsonValue>>* RoomsArr = nullptr;
	if (!Json->TryGetArrayField(TEXT("rooms"), RoomsArr) || !RoomsArr)
	{
		OutError = TEXT("Archetype missing 'rooms' array");
		return false;
	}

	for (int32 i = 0; i < RoomsArr->Num(); ++i)
	{
		const TSharedPtr<FJsonObject>* RObj = nullptr;
		if (!(*RoomsArr)[i]->TryGetObject(RObj) || !RObj || !(*RObj).IsValid())
			continue;

		FArchetypeRoom Room;
		(*RObj)->TryGetStringField(TEXT("type"), Room.Type);

		if ((*RObj)->HasField(TEXT("min_area")))
			Room.MinArea = static_cast<float>((*RObj)->GetNumberField(TEXT("min_area")));
		if ((*RObj)->HasField(TEXT("max_area")))
			Room.MaxArea = static_cast<float>((*RObj)->GetNumberField(TEXT("max_area")));

		// Count can be a number or an array [min, max]
		if ((*RObj)->HasField(TEXT("count")))
		{
			const TArray<TSharedPtr<FJsonValue>>* CountArr = nullptr;
			if ((*RObj)->TryGetArrayField(TEXT("count"), CountArr) && CountArr && CountArr->Num() >= 2)
			{
				Room.CountMin = static_cast<int32>((*CountArr)[0]->AsNumber());
				Room.CountMax = static_cast<int32>((*CountArr)[1]->AsNumber());
			}
			else
			{
				int32 CountVal = static_cast<int32>((*RObj)->GetNumberField(TEXT("count")));
				Room.CountMin = CountVal;
				Room.CountMax = CountVal;
			}
		}

		if ((*RObj)->HasField(TEXT("required")))
			Room.bRequired = (*RObj)->GetBoolField(TEXT("required"));
		if ((*RObj)->HasField(TEXT("priority")))
			Room.Priority = static_cast<int32>((*RObj)->GetNumberField(TEXT("priority")));
		if ((*RObj)->HasField(TEXT("auto_generate")))
			Room.bAutoGenerate = (*RObj)->GetBoolField(TEXT("auto_generate"));
		if ((*RObj)->HasField(TEXT("exterior_wall")))
			Room.bExteriorWall = (*RObj)->GetBoolField(TEXT("exterior_wall"));

		// Per-floor assignment
		if ((*RObj)->HasField(TEXT("floor")))
			(*RObj)->TryGetStringField(TEXT("floor"), Room.Floor);

		// Aspect ratio constraints
		if ((*RObj)->HasField(TEXT("min_aspect")))
			Room.MinAspect = static_cast<float>((*RObj)->GetNumberField(TEXT("min_aspect")));
		if ((*RObj)->HasField(TEXT("max_aspect")))
			Room.MaxAspect = static_cast<float>((*RObj)->GetNumberField(TEXT("max_aspect")));

		OutArchetype.Rooms.Add(MoveTemp(Room));
	}

	// Parse adjacency
	const TArray<TSharedPtr<FJsonValue>>* AdjArr = nullptr;
	if (Json->TryGetArrayField(TEXT("adjacency"), AdjArr) && AdjArr)
	{
		for (const auto& AdjVal : *AdjArr)
		{
			const TSharedPtr<FJsonObject>* AObj = nullptr;
			if (!AdjVal->TryGetObject(AObj) || !AObj || !(*AObj).IsValid())
				continue;

			FAdjacencyRule Rule;
			(*AObj)->TryGetStringField(TEXT("from"), Rule.From);
			(*AObj)->TryGetStringField(TEXT("to"), Rule.To);
			(*AObj)->TryGetStringField(TEXT("strength"), Rule.Strength);
			OutArchetype.Adjacency.Add(MoveTemp(Rule));
		}
	}

	if (OutArchetype.Name.IsEmpty())
	{
		OutError = TEXT("Archetype missing 'name' field");
		return false;
	}

	return true;
}

// ============================================================================
// Room Resolution
// ============================================================================

TArray<FMonolithMeshFloorPlanGenerator::FRoomInstance> FMonolithMeshFloorPlanGenerator::ResolveRoomInstances(
	const FBuildingArchetype& Archetype, int32 GridW, int32 GridH, FRandomStream& Rng, int32 FloorIndex)
{
	TArray<FRoomInstance> Instances;
	const int32 TotalCells = GridW * GridH;

	for (const FArchetypeRoom& AR : Archetype.Rooms)
	{
		if (AR.bAutoGenerate)
			continue;  // Corridors are generated later

		// Per-floor filtering: skip rooms that don't belong on this floor
		if (FloorIndex >= 0)
		{
			const FString& RoomFloor = AR.Floor;
			if (RoomFloor == TEXT("ground") && FloorIndex != 0)
				continue;
			if (RoomFloor == TEXT("upper") && FloorIndex == 0)
				continue;
			// "every" and "any" (default) pass through on all floors
		}

		// Roll how many of this room type
		int32 Count = Rng.RandRange(AR.CountMin, AR.CountMax);

		for (int32 i = 0; i < Count; ++i)
		{
			FRoomInstance Inst;
			Inst.RoomType = AR.Type;
			Inst.RoomId = (Count > 1) ? FString::Printf(TEXT("%s_%d"), *AR.Type, i + 1) : AR.Type;
			Inst.Priority = AR.Priority;
			Inst.bExteriorWall = AR.bExteriorWall;
			Inst.Floor = AR.Floor;
			Inst.MinAspect = AR.MinAspect;
			Inst.MaxAspect = AR.MaxAspect;

			// Target area: random within range, clamped to available grid space
			float Area = Rng.FRandRange(AR.MinArea, AR.MaxArea);
			// Clamp to a reasonable fraction of the total
			Area = FMath::Clamp(Area, 1.0f, static_cast<float>(TotalCells) * 0.6f);
			Inst.TargetArea = Area;

			Instances.Add(MoveTemp(Inst));
		}
	}

	// Sort by priority (lower number = higher priority = gets laid out first = better aspect ratio)
	Instances.Sort([](const FRoomInstance& A, const FRoomInstance& B) { return A.Priority < B.Priority; });

	// Normalize areas so they sum to the total grid area
	float AreaSum = 0.0f;
	for (const FRoomInstance& Inst : Instances)
		AreaSum += Inst.TargetArea;

	if (AreaSum > 0.0f)
	{
		// Reserve ~15% for corridors
		const float UsableArea = static_cast<float>(TotalCells) * 0.85f;
		const float Scale = UsableArea / AreaSum;
		for (FRoomInstance& Inst : Instances)
		{
			Inst.TargetArea = FMath::Max(1.0f, Inst.TargetArea * Scale);
		}
	}

	return Instances;
}

// ============================================================================
// Validation
// ============================================================================

FString FMonolithMeshFloorPlanGenerator::ValidateFootprintCapacity(
	const FBuildingArchetype& Archetype, int32 GridW, int32 GridH, int32 FloorIndex)
{
	float TotalRequiredMin = 0.0f;
	for (const FArchetypeRoom& AR : Archetype.Rooms)
	{
		if (AR.bAutoGenerate)
			continue;
		if (!AR.bRequired)
			continue;

		// Per-floor filtering
		if (FloorIndex >= 0)
		{
			if (AR.Floor == TEXT("ground") && FloorIndex != 0)
				continue;
			if (AR.Floor == TEXT("upper") && FloorIndex == 0)
				continue;
		}

		TotalRequiredMin += AR.MinArea * static_cast<float>(AR.CountMin);
	}

	const float AvailableArea = static_cast<float>(GridW * GridH) * 0.85f;  // 85% after corridors
	if (TotalRequiredMin > AvailableArea)
	{
		// Convert to meters for a helpful error message
		float NeededM2 = TotalRequiredMin * 0.25f / 0.85f;  // Account for corridor reservation
		float NeededSide = FMath::Sqrt(NeededM2);
		float NeededCm = NeededSide * 100.0f;
		return FString::Printf(
			TEXT("Footprint too small for archetype '%s'. Required rooms need at least %.0f grid cells (%.0f m²) "
				"but footprint provides %.0f usable cells (%.0f m²). Try at least %.0fx%.0f cm."),
			*Archetype.Name,
			TotalRequiredMin, TotalRequiredMin * 0.25f,
			AvailableArea, AvailableArea * 0.25f,
			NeededCm, NeededCm);
	}

	return FString();
}

FString FMonolithMeshFloorPlanGenerator::ValidateStairwellRequirement(const FBuildingArchetype& Archetype)
{
	if (Archetype.FloorsMax <= 1)
		return FString();  // Single-floor archetypes don't need stairwells

	bool bHasStairwell = false;
	for (const FArchetypeRoom& AR : Archetype.Rooms)
	{
		if (AR.Type == TEXT("stairwell") || AR.Type == TEXT("stairs"))
		{
			bHasStairwell = true;
			if (AR.MinArea < 24.0f)
			{
				return FString::Printf(
					TEXT("Multi-floor archetype '%s' has stairwell with min_area %.0f but minimum is 24 cells "
						"(4x6 = 200x300cm). Increase stairwell min_area."),
					*Archetype.Name, AR.MinArea);
			}
			break;
		}
	}

	if (!bHasStairwell)
	{
		return FString::Printf(
			TEXT("Multi-floor archetype '%s' (%d-%d floors) has no stairwell room type. "
				"Add a room with type 'stairwell', floor 'every', and min_area >= 24."),
			*Archetype.Name, Archetype.FloorsMin, Archetype.FloorsMax);
	}

	return FString();
}

// ============================================================================
// Aspect Ratio Correction
// ============================================================================

void FMonolithMeshFloorPlanGenerator::CorrectAspectRatios(
	TArray<FGridRect>& Rects, const TArray<FRoomInstance>& Rooms, int32 GridW, int32 GridH)
{
	for (int32 i = 0; i < Rects.Num() && i < Rooms.Num(); ++i)
	{
		FGridRect& R = Rects[i];
		if (R.W <= 0 || R.H <= 0)
			continue;

		float MaxAspect = Rooms[i].MaxAspect;
		float CurrentAspect = R.AspectRatio();

		if (CurrentAspect <= MaxAspect)
			continue;

		// Room exceeds max aspect ratio. Try to reshape it.
		// Strategy: reduce the longer dimension and increase the shorter one,
		// keeping area roughly the same (within grid constraints).
		int32 Area = R.Area();
		bool bWideRoom = R.W > R.H;

		// Calculate ideal dimensions for the target aspect ratio
		// Area = W * H, W/H = MaxAspect => W = sqrt(Area * MaxAspect), H = sqrt(Area / MaxAspect)
		int32 IdealW, IdealH;
		if (bWideRoom)
		{
			IdealW = FMath::RoundToInt32(FMath::Sqrt(static_cast<float>(Area) * MaxAspect));
			IdealH = FMath::Max(1, FMath::RoundToInt32(static_cast<float>(Area) / static_cast<float>(IdealW)));
		}
		else
		{
			IdealH = FMath::RoundToInt32(FMath::Sqrt(static_cast<float>(Area) * MaxAspect));
			IdealW = FMath::Max(1, FMath::RoundToInt32(static_cast<float>(Area) / static_cast<float>(IdealH)));
		}

		// Clamp to grid bounds
		IdealW = FMath::Clamp(IdealW, 1, GridW - R.X);
		IdealH = FMath::Clamp(IdealH, 1, GridH - R.Y);

		// Only apply if the new aspect ratio is actually better
		float NewAspect = (IdealW > 0 && IdealH > 0)
			? FMath::Max(static_cast<float>(IdealW) / IdealH, static_cast<float>(IdealH) / IdealW)
			: CurrentAspect;

		if (NewAspect < CurrentAspect)
		{
			UE_LOG(LogMonolithFloorPlan, Log, TEXT("Correcting aspect ratio for '%s': %dx%d (%.1f) -> %dx%d (%.1f)"),
				*Rooms[i].RoomId, R.W, R.H, CurrentAspect, IdealW, IdealH, NewAspect);
			R.W = IdealW;
			R.H = IdealH;
		}
	}
}

// ============================================================================
// Squarified Treemap
// ============================================================================

float FMonolithMeshFloorPlanGenerator::WorstAspectRatio(const TArray<float>& RowAreas, float SideLength)
{
	if (RowAreas.Num() == 0 || SideLength <= 0.0f)
		return TNumericLimits<float>::Max();

	float RowTotal = 0.0f;
	for (float A : RowAreas)
		RowTotal += A;

	if (RowTotal <= 0.0f)
		return TNumericLimits<float>::Max();

	float RowWidth = RowTotal / SideLength;
	float Worst = 0.0f;

	for (float A : RowAreas)
	{
		if (A <= 0.0f) continue;
		float H = A / RowWidth;
		float Ratio = FMath::Max(RowWidth / H, H / RowWidth);
		Worst = FMath::Max(Worst, Ratio);
	}

	return Worst;
}

void FMonolithMeshFloorPlanGenerator::LayoutRow(const TArray<int32>& RowIndices, const TArray<float>& Areas,
	FGridRect& Rect, bool bHorizontal, TArray<FGridRect>& OutRects)
{
	if (RowIndices.Num() == 0) return;

	float RowTotal = 0.0f;
	for (int32 Idx : RowIndices)
		RowTotal += Areas[Idx];

	if (bHorizontal)
	{
		// Row is laid out along the X axis, consuming some height
		int32 RowHeight = FMath::Max(1, FMath::RoundToInt32(RowTotal / static_cast<float>(Rect.W)));
		RowHeight = FMath::Min(RowHeight, Rect.H);

		int32 CurX = Rect.X;
		for (int32 i = 0; i < RowIndices.Num(); ++i)
		{
			int32 Idx = RowIndices[i];
			int32 CellW;
			if (i == RowIndices.Num() - 1)
			{
				// Last item gets remaining width to avoid rounding gaps
				CellW = (Rect.X + Rect.W) - CurX;
			}
			else
			{
				CellW = FMath::Max(1, FMath::RoundToInt32(Areas[Idx] / static_cast<float>(RowHeight)));
				CellW = FMath::Min(CellW, (Rect.X + Rect.W) - CurX);
			}

			FGridRect& R = OutRects[Idx];
			R.X = CurX;
			R.Y = Rect.Y;
			R.W = CellW;
			R.H = RowHeight;
			CurX += CellW;
		}

		// Shrink remaining rect
		Rect.Y += RowHeight;
		Rect.H -= RowHeight;
	}
	else
	{
		// Row is laid out along the Y axis, consuming some width
		int32 RowWidth = FMath::Max(1, FMath::RoundToInt32(RowTotal / static_cast<float>(Rect.H)));
		RowWidth = FMath::Min(RowWidth, Rect.W);

		int32 CurY = Rect.Y;
		for (int32 i = 0; i < RowIndices.Num(); ++i)
		{
			int32 Idx = RowIndices[i];
			int32 CellH;
			if (i == RowIndices.Num() - 1)
			{
				CellH = (Rect.Y + Rect.H) - CurY;
			}
			else
			{
				CellH = FMath::Max(1, FMath::RoundToInt32(Areas[Idx] / static_cast<float>(RowWidth)));
				CellH = FMath::Min(CellH, (Rect.Y + Rect.H) - CurY);
			}

			FGridRect& R = OutRects[Idx];
			R.X = Rect.X;
			R.Y = CurY;
			R.W = RowWidth;
			R.H = CellH;
			CurY += CellH;
		}

		// Shrink remaining rect
		Rect.X += RowWidth;
		Rect.W -= RowWidth;
	}
}

TArray<FMonolithMeshFloorPlanGenerator::FGridRect> FMonolithMeshFloorPlanGenerator::SquarifiedTreemapLayout(
	TArray<FRoomInstance>& Rooms, int32 GridW, int32 GridH)
{
	TArray<FGridRect> Rects;
	Rects.SetNum(Rooms.Num());

	if (Rooms.Num() == 0)
		return Rects;

	// Build area array (already sorted by priority from ResolveRoomInstances)
	// Re-sort by area descending for treemap algorithm
	TArray<int32> SortedIndices;
	SortedIndices.Reserve(Rooms.Num());
	for (int32 i = 0; i < Rooms.Num(); ++i)
		SortedIndices.Add(i);

	SortedIndices.Sort([&Rooms](int32 A, int32 B) { return Rooms[A].TargetArea > Rooms[B].TargetArea; });

	TArray<float> Areas;
	Areas.SetNum(Rooms.Num());
	for (int32 i = 0; i < Rooms.Num(); ++i)
		Areas[i] = Rooms[i].TargetArea;

	FGridRect Remaining;
	Remaining.X = 0;
	Remaining.Y = 0;
	Remaining.W = GridW;
	Remaining.H = GridH;

	TArray<int32> CurrentRow;
	TArray<float> CurrentRowAreas;
	int32 Cursor = 0;

	while (Cursor < SortedIndices.Num())
	{
		if (Remaining.W <= 0 || Remaining.H <= 0)
		{
			// No space left -- force remaining rooms into last pixel
			for (int32 i = Cursor; i < SortedIndices.Num(); ++i)
			{
				FGridRect& R = Rects[SortedIndices[i]];
				R.X = FMath::Max(0, Remaining.X);
				R.Y = FMath::Max(0, Remaining.Y);
				R.W = FMath::Max(1, Remaining.W);
				R.H = FMath::Max(1, Remaining.H);
			}
			break;
		}

		bool bHorizontal = Remaining.W >= Remaining.H;
		float SideLength = bHorizontal ? static_cast<float>(Remaining.W) : static_cast<float>(Remaining.H);

		int32 Idx = SortedIndices[Cursor];

		// Try adding this room to the current row
		TArray<float> CandidateAreas = CurrentRowAreas;
		CandidateAreas.Add(Areas[Idx]);

		float CurrentWorst = (CurrentRowAreas.Num() > 0)
			? WorstAspectRatio(CurrentRowAreas, SideLength)
			: TNumericLimits<float>::Max();
		float CandidateWorst = WorstAspectRatio(CandidateAreas, SideLength);

		if (CandidateWorst <= CurrentWorst || CurrentRow.Num() == 0)
		{
			// Adding improves (or doesn't worsen) aspect ratios -- keep going
			CurrentRow.Add(Idx);
			CurrentRowAreas.Add(Areas[Idx]);
			++Cursor;
		}
		else
		{
			// Finalize current row
			LayoutRow(CurrentRow, Areas, Remaining, bHorizontal, Rects);
			CurrentRow.Reset();
			CurrentRowAreas.Reset();
			// Don't advance cursor -- re-evaluate this room against the new remaining rect
		}
	}

	// Lay out any remaining row
	if (CurrentRow.Num() > 0 && Remaining.W > 0 && Remaining.H > 0)
	{
		bool bHorizontal = Remaining.W >= Remaining.H;
		LayoutRow(CurrentRow, Areas, Remaining, bHorizontal, Rects);
	}

	return Rects;
}

// ============================================================================
// Grid construction
// ============================================================================

TArray<TArray<int32>> FMonolithMeshFloorPlanGenerator::BuildGridFromRects(
	const TArray<FGridRect>& Rects, int32 GridW, int32 GridH)
{
	// Initialize with -1 (empty)
	TArray<TArray<int32>> Grid;
	Grid.SetNum(GridH);
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		Grid[Y].SetNum(GridW);
		for (int32 X = 0; X < GridW; ++X)
			Grid[Y][X] = -1;
	}

	// Paint rooms. Later rooms overwrite earlier ones at overlaps (shouldn't happen with treemap).
	for (int32 RoomIdx = 0; RoomIdx < Rects.Num(); ++RoomIdx)
	{
		const FGridRect& R = Rects[RoomIdx];
		for (int32 Y = R.Y; Y < R.Y + R.H && Y < GridH; ++Y)
		{
			for (int32 X = R.X; X < R.X + R.W && X < GridW; ++X)
			{
				Grid[Y][X] = RoomIdx;
			}
		}
	}

	return Grid;
}

TArray<FRoomDef> FMonolithMeshFloorPlanGenerator::BuildRoomDefs(
	const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	const TArray<FRoomInstance>& Instances)
{
	TArray<FRoomDef> Rooms;
	Rooms.SetNum(Instances.Num());

	for (int32 i = 0; i < Instances.Num(); ++i)
	{
		Rooms[i].RoomId = Instances[i].RoomId;
		Rooms[i].RoomType = Instances[i].RoomType;
	}

	// Collect grid cells for each room
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			int32 Idx = Grid[Y][X];
			if (Idx >= 0 && Idx < Rooms.Num())
			{
				Rooms[Idx].GridCells.Add(FIntPoint(X, Y));
			}
		}
	}

	return Rooms;
}

// ============================================================================
// Corridor insertion
// ============================================================================

bool FMonolithMeshFloorPlanGenerator::RoomsShareEdge(const FRoomDef& A, const FRoomDef& B)
{
	// Two rooms share an edge if any cell in A is cardinally adjacent to any cell in B
	TSet<FIntPoint> SetB;
	for (const FIntPoint& P : B.GridCells)
		SetB.Add(P);

	static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
	for (const FIntPoint& P : A.GridCells)
	{
		for (const FIntPoint& D : Dirs)
		{
			if (SetB.Contains(P + D))
				return true;
		}
	}
	return false;
}

TArray<FIntPoint> FMonolithMeshFloorPlanGenerator::FindCorridorPath(
	const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	const FRoomDef& From, const FRoomDef& To, int32 CorridorRoomIndex, int32 CorridorWidth)
{
	TArray<FIntPoint> Path;

	// BFS from any cell adjacent to From toward any cell adjacent to To
	// through empty (-1) or existing corridor cells

	TSet<FIntPoint> FromEdge, ToEdge;
	static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

	auto IsPassable = [&](int32 X, int32 Y) -> bool
	{
		if (X < 0 || Y < 0 || X >= GridW || Y >= GridH) return false;
		int32 Val = Grid[Y][X];
		return Val == -1 || Val == CorridorRoomIndex;
	};

	// Find cells adjacent to room From that are passable
	for (const FIntPoint& P : From.GridCells)
	{
		for (const FIntPoint& D : Dirs)
		{
			FIntPoint N = P + D;
			if (N.X >= 0 && N.Y >= 0 && N.X < GridW && N.Y < GridH && IsPassable(N.X, N.Y))
				FromEdge.Add(N);
		}
	}

	// Find cells that are part of To or adjacent to To
	TSet<FIntPoint> ToSet;
	for (const FIntPoint& P : To.GridCells)
		ToSet.Add(P);

	for (const FIntPoint& P : To.GridCells)
	{
		for (const FIntPoint& D : Dirs)
		{
			FIntPoint N = P + D;
			if (IsPassable(N.X, N.Y))
				ToEdge.Add(N);
		}
	}

	if (FromEdge.Num() == 0 || ToEdge.Num() == 0)
		return Path;

	// BFS
	TMap<FIntPoint, FIntPoint> CameFrom;  // child -> parent
	TQueue<FIntPoint> Queue;

	for (const FIntPoint& Start : FromEdge)
	{
		Queue.Enqueue(Start);
		CameFrom.Add(Start, FIntPoint(-1, -1));
	}

	FIntPoint Goal(-1, -1);
	while (!Queue.IsEmpty())
	{
		FIntPoint Current;
		Queue.Dequeue(Current);

		// Check if we reached a cell adjacent to the target room
		if (ToEdge.Contains(Current) || ToSet.Contains(Current))
		{
			Goal = Current;
			break;
		}

		for (const FIntPoint& D : Dirs)
		{
			FIntPoint N = Current + D;
			if (!IsPassable(N.X, N.Y) && !ToSet.Contains(N))
				continue;
			if (CameFrom.Contains(N))
				continue;

			CameFrom.Add(N, Current);
			Queue.Enqueue(N);
		}
	}

	if (Goal.X < 0)
		return Path;  // No path found

	// Reconstruct path
	FIntPoint Cur = Goal;
	while (Cur.X >= 0 && Cur.Y >= 0)
	{
		// Don't include cells that belong to the target room
		if (!ToSet.Contains(Cur))
			Path.Add(Cur);

		FIntPoint* Parent = CameFrom.Find(Cur);
		if (!Parent || (Parent->X < 0 && Parent->Y < 0))
			break;
		Cur = *Parent;
	}

	// Widen corridor if requested (for hospice mode)
	if (CorridorWidth > 1)
	{
		TSet<FIntPoint> Widened;
		for (const FIntPoint& P : Path)
			Widened.Add(P);

		for (const FIntPoint& P : Path)
		{
			// Add cells in the perpendicular direction
			for (int32 Offset = 1; Offset < CorridorWidth; ++Offset)
			{
				// Try both perpendicular directions
				FIntPoint W1(P.X + Offset, P.Y);
				FIntPoint W2(P.X, P.Y + Offset);

				if (W1.X < GridW && (Grid[W1.Y][W1.X] == -1 || Grid[W1.Y][W1.X] == CorridorRoomIndex))
					Widened.Add(W1);
				if (W2.Y < GridH && (Grid[W2.Y][W2.X] == -1 || Grid[W2.Y][W2.X] == CorridorRoomIndex))
					Widened.Add(W2);
			}
		}

		Path = Widened.Array();
	}

	return Path;
}

void FMonolithMeshFloorPlanGenerator::InsertCorridors(
	TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	TArray<FRoomDef>& Rooms, const TArray<FAdjacencyRule>& Adjacency,
	bool bHospiceMode, FRandomStream& Rng)
{
	// Build a map from room type -> room indices (handles multiple instances like bedroom_1, bedroom_2)
	TMap<FString, TArray<int32>> TypeToIndices;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		TypeToIndices.FindOrAdd(Rooms[i].RoomType).Add(i);
	}

	// Determine corridor room index (always the next available)
	int32 CorridorRoomIndex = Rooms.Num();

	// Collect all room pairs that need connectivity but don't share an edge
	TArray<TPair<int32, int32>> NeedsCorridor;

	for (const FAdjacencyRule& Rule : Adjacency)
	{
		if (Rule.Strength == TEXT("weak"))
			continue;  // Weak adjacencies don't need corridors

		const TArray<int32>* FromIndices = TypeToIndices.Find(Rule.From);
		const TArray<int32>* ToIndices = TypeToIndices.Find(Rule.To);

		if (!FromIndices || !ToIndices)
			continue;

		for (int32 Fi : *FromIndices)
		{
			for (int32 Ti : *ToIndices)
			{
				if (Fi == Ti) continue;
				if (!RoomsShareEdge(Rooms[Fi], Rooms[Ti]))
				{
					NeedsCorridor.Add(TPair<int32, int32>(Fi, Ti));
				}
			}
		}
	}

	if (NeedsCorridor.Num() == 0)
	{
		// All required adjacencies are satisfied -- still add a minimal corridor for circulation
		// Find the largest room and carve a corridor strip along its longest interior edge
		return;
	}

	// Create the corridor room definition
	FRoomDef CorridorRoom;
	CorridorRoom.RoomId = TEXT("corridor");
	CorridorRoom.RoomType = TEXT("corridor");

	int32 CorridorWidth = bHospiceMode ? 4 : 2;  // 4 cells * 50cm = 200cm > 180cm min; 2 cells = 100cm normal

	// Carve corridors for each pair
	for (const auto& Pair : NeedsCorridor)
	{
		TArray<FIntPoint> Path = FindCorridorPath(Grid, GridW, GridH, Rooms[Pair.Key], Rooms[Pair.Value], CorridorRoomIndex, CorridorWidth);

		for (const FIntPoint& P : Path)
		{
			if (P.X >= 0 && P.Y >= 0 && P.X < GridW && P.Y < GridH)
			{
				int32 Current = Grid[P.Y][P.X];
				if (Current == -1 || Current == CorridorRoomIndex)
				{
					Grid[P.Y][P.X] = CorridorRoomIndex;
					CorridorRoom.GridCells.AddUnique(P);
				}
			}
		}
	}

	// Also ensure all rooms are reachable from the corridor.
	// For any room not adjacent to the corridor, carve a path.
	if (CorridorRoom.GridCells.Num() > 0)
	{
		for (int32 i = 0; i < Rooms.Num(); ++i)
		{
			if (!RoomsShareEdge(Rooms[i], CorridorRoom))
			{
				TArray<FIntPoint> Path = FindCorridorPath(Grid, GridW, GridH, Rooms[i], CorridorRoom, CorridorRoomIndex, CorridorWidth);
				for (const FIntPoint& P : Path)
				{
					if (P.X >= 0 && P.Y >= 0 && P.X < GridW && P.Y < GridH)
					{
						int32 Current = Grid[P.Y][P.X];
						if (Current == -1 || Current == CorridorRoomIndex)
						{
							Grid[P.Y][P.X] = CorridorRoomIndex;
							CorridorRoom.GridCells.AddUnique(P);
						}
					}
				}
			}
		}
	}

	// Only add the corridor room if it has any cells
	if (CorridorRoom.GridCells.Num() > 0)
	{
		Rooms.Add(MoveTemp(CorridorRoom));
	}

	// Also claim any remaining empty cells adjacent to corridors (prevents isolated pockets)
	bool bChanged = true;
	int32 Passes = 0;
	while (bChanged && Passes < 3)
	{
		bChanged = false;
		++Passes;
		static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
		for (int32 Y = 0; Y < GridH; ++Y)
		{
			for (int32 X = 0; X < GridW; ++X)
			{
				if (Grid[Y][X] != -1) continue;

				// Count adjacent corridor cells
				int32 AdjCorridor = 0;
				for (const FIntPoint& D : Dirs)
				{
					int32 NX = X + D.X, NY = Y + D.Y;
					if (NX >= 0 && NY >= 0 && NX < GridW && NY < GridH && Grid[NY][NX] == CorridorRoomIndex)
						++AdjCorridor;
				}

				if (AdjCorridor >= 2)
				{
					Grid[Y][X] = CorridorRoomIndex;
					if (Rooms.Num() > CorridorRoomIndex)
						Rooms[CorridorRoomIndex].GridCells.Add(FIntPoint(X, Y));
					bChanged = true;
				}
			}
		}
	}
}

// ============================================================================
// Hospice rest alcoves
// ============================================================================

void FMonolithMeshFloorPlanGenerator::InsertRestAlcoves(
	TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	TArray<FRoomDef>& Rooms, int32 RoomsBetweenAlcoves, FRandomStream& Rng)
{
	// Find corridor room index
	int32 CorridorIdx = -1;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		if (Rooms[i].RoomType == TEXT("corridor"))
		{
			CorridorIdx = i;
			break;
		}
	}

	if (CorridorIdx < 0)
		return;  // No corridor to attach alcoves to

	// Count non-corridor rooms
	int32 RealRoomCount = 0;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		if (Rooms[i].RoomType != TEXT("corridor") && Rooms[i].RoomType != TEXT("rest_alcove"))
			++RealRoomCount;
	}

	int32 AlcoveCount = FMath::Max(1, RealRoomCount / RoomsBetweenAlcoves);
	static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

	for (int32 A = 0; A < AlcoveCount; ++A)
	{
		// Pick a random corridor cell and try to carve an alcove next to it
		if (Rooms[CorridorIdx].GridCells.Num() == 0)
			break;

		for (int32 Attempt = 0; Attempt < 20; ++Attempt)
		{
			int32 CellIdx = Rng.RandRange(0, Rooms[CorridorIdx].GridCells.Num() - 1);
			FIntPoint Base = Rooms[CorridorIdx].GridCells[CellIdx];

			// Try each direction for a 2x2 alcove
			for (const FIntPoint& D : Dirs)
			{
				FIntPoint P1 = Base + D;
				FIntPoint P2 = P1 + FIntPoint(D.Y != 0 ? 1 : 0, D.X != 0 ? 1 : 0);  // Perpendicular

				if (P1.X < 0 || P1.Y < 0 || P1.X >= GridW || P1.Y >= GridH) continue;
				if (P2.X < 0 || P2.Y < 0 || P2.X >= GridW || P2.Y >= GridH) continue;
				if (Grid[P1.Y][P1.X] != -1 || Grid[P2.Y][P2.X] != -1) continue;

				// Found space for an alcove
				int32 AlcoveIdx = Rooms.Num();
				FRoomDef Alcove;
				Alcove.RoomId = FString::Printf(TEXT("rest_alcove_%d"), A + 1);
				Alcove.RoomType = TEXT("rest_alcove");
				Alcove.GridCells.Add(P1);
				Alcove.GridCells.Add(P2);

				Grid[P1.Y][P1.X] = AlcoveIdx;
				Grid[P2.Y][P2.X] = AlcoveIdx;
				Rooms.Add(MoveTemp(Alcove));
				goto NextAlcove;
			}
		}
		NextAlcove:;
	}
}

// ============================================================================
// Door placement
// ============================================================================

TArray<FIntPoint> FMonolithMeshFloorPlanGenerator::FindSharedEdge(
	const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	int32 RoomIndexA, int32 RoomIndexB)
{
	TArray<FIntPoint> SharedCells;
	static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

	for (int32 Y = 0; Y < GridH; ++Y)
	{
		for (int32 X = 0; X < GridW; ++X)
		{
			if (Grid[Y][X] != RoomIndexA) continue;

			for (const FIntPoint& D : Dirs)
			{
				int32 NX = X + D.X, NY = Y + D.Y;
				if (NX >= 0 && NY >= 0 && NX < GridW && NY < GridH && Grid[NY][NX] == RoomIndexB)
				{
					SharedCells.Add(FIntPoint(X, Y));
					break;  // Only add each cell once
				}
			}
		}
	}

	return SharedCells;
}

TArray<FDoorDef> FMonolithMeshFloorPlanGenerator::PlaceDoors(
	const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	const TArray<FRoomDef>& Rooms, const TArray<FAdjacencyRule>& Adjacency,
	bool bHospiceMode, FRandomStream& Rng)
{
	TArray<FDoorDef> Doors;

	// Build type -> indices map
	TMap<FString, TArray<int32>> TypeToIndices;
	for (int32 i = 0; i < Rooms.Num(); ++i)
		TypeToIndices.FindOrAdd(Rooms[i].RoomType).Add(i);

	// Track which room pairs already have doors to avoid duplicates
	TSet<uint64> DoorPairs;
	auto PairKey = [](int32 A, int32 B) -> uint64
	{
		int32 Lo = FMath::Min(A, B);
		int32 Hi = FMath::Max(A, B);
		return (static_cast<uint64>(Lo) << 32) | static_cast<uint64>(Hi);
	};

	int32 DoorCounter = 0;
	float DoorWidth = bHospiceMode ? 100.0f : 90.0f;

	// Place doors for each adjacency rule
	for (const FAdjacencyRule& Rule : Adjacency)
	{
		const TArray<int32>* FromIndices = TypeToIndices.Find(Rule.From);
		const TArray<int32>* ToIndices = TypeToIndices.Find(Rule.To);
		if (!FromIndices || !ToIndices) continue;

		for (int32 Fi : *FromIndices)
		{
			for (int32 Ti : *ToIndices)
			{
				if (Fi == Ti) continue;
				uint64 Key = PairKey(Fi, Ti);
				if (DoorPairs.Contains(Key)) continue;

				TArray<FIntPoint> SharedEdge = FindSharedEdge(Grid, GridW, GridH, Fi, Ti);
				if (SharedEdge.Num() == 0) continue;

				DoorPairs.Add(Key);

				// Pick a cell roughly in the middle of the shared edge for door placement
				int32 DoorCellIdx = SharedEdge.Num() / 2;
				FIntPoint DoorCell = SharedEdge[DoorCellIdx];

				// Determine which direction the door faces
				FIntPoint NeighborCell(INDEX_NONE, INDEX_NONE);
				static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
				FString WallDir;
				for (const FIntPoint& D : Dirs)
				{
					int32 NX = DoorCell.X + D.X, NY = DoorCell.Y + D.Y;
					if (NX >= 0 && NY >= 0 && NX < GridW && NY < GridH && Grid[NY][NX] == Ti)
					{
						NeighborCell = FIntPoint(NX, NY);
						if (D.X > 0) WallDir = TEXT("east");
						else if (D.X < 0) WallDir = TEXT("west");
						else if (D.Y > 0) WallDir = TEXT("south");
						else WallDir = TEXT("north");
						break;
					}
				}

				FDoorDef Door;
				Door.DoorId = FString::Printf(TEXT("door_%02d"), ++DoorCounter);
				Door.RoomA = Rooms[Fi].RoomId;
				Door.RoomB = Rooms[Ti].RoomId;
				Door.EdgeStart = DoorCell;
				Door.EdgeEnd = (NeighborCell.X != INDEX_NONE) ? NeighborCell : DoorCell;
				Door.Wall = WallDir;
				Door.Width = DoorWidth;
				Door.Height = 220.0f;

				Doors.Add(MoveTemp(Door));
			}
		}
	}

	// Ensure corridor connects to every room that has a shared edge with it
	int32 CorridorIdx = -1;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		if (Rooms[i].RoomType == TEXT("corridor"))
		{
			CorridorIdx = i;
			break;
		}
	}

	if (CorridorIdx >= 0)
	{
		for (int32 i = 0; i < Rooms.Num(); ++i)
		{
			if (i == CorridorIdx) continue;
			if (Rooms[i].RoomType == TEXT("corridor")) continue;

			uint64 Key = PairKey(i, CorridorIdx);
			if (DoorPairs.Contains(Key)) continue;

			TArray<FIntPoint> SharedEdge = FindSharedEdge(Grid, GridW, GridH, i, CorridorIdx);
			if (SharedEdge.Num() == 0) continue;

			DoorPairs.Add(Key);

			int32 DoorCellIdx = SharedEdge.Num() / 2;
			FIntPoint DoorCell = SharedEdge[DoorCellIdx];

			FIntPoint NeighborCell(INDEX_NONE, INDEX_NONE);
			static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
			FString WallDir;
			for (const FIntPoint& D : Dirs)
			{
				int32 NX = DoorCell.X + D.X, NY = DoorCell.Y + D.Y;
				if (NX >= 0 && NY >= 0 && NX < GridW && NY < GridH && Grid[NY][NX] == CorridorIdx)
				{
					NeighborCell = FIntPoint(NX, NY);
					if (D.X > 0) WallDir = TEXT("east");
					else if (D.X < 0) WallDir = TEXT("west");
					else if (D.Y > 0) WallDir = TEXT("south");
					else WallDir = TEXT("north");
					break;
				}
			}

			FDoorDef Door;
			Door.DoorId = FString::Printf(TEXT("door_%02d"), ++DoorCounter);
			Door.RoomA = Rooms[i].RoomId;
			Door.RoomB = TEXT("corridor");
			Door.EdgeStart = DoorCell;
			Door.EdgeEnd = (NeighborCell.X != INDEX_NONE) ? NeighborCell : DoorCell;
			Door.Wall = WallDir;
			Door.Width = DoorWidth;
			Door.Height = 220.0f;

			Doors.Add(MoveTemp(Door));
		}
	}

	// For rooms that still have no door at all, add one to the closest neighbor
	TSet<int32> RoomsWithDoors;
	for (const FDoorDef& D : Doors)
	{
		for (int32 i = 0; i < Rooms.Num(); ++i)
		{
			if (Rooms[i].RoomId == D.RoomA || Rooms[i].RoomId == D.RoomB)
				RoomsWithDoors.Add(i);
		}
	}

	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		if (RoomsWithDoors.Contains(i)) continue;

		// Find any adjacent room and add a door
		static const FIntPoint Dirs[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
		for (const FIntPoint& Cell : Rooms[i].GridCells)
		{
			bool bFoundDoor = false;
			for (const FIntPoint& D : Dirs)
			{
				int32 NX = Cell.X + D.X, NY = Cell.Y + D.Y;
				if (NX < 0 || NY < 0 || NX >= GridW || NY >= GridH) continue;
				int32 Neighbor = Grid[NY][NX];
				if (Neighbor >= 0 && Neighbor != i)
				{
					uint64 Key = PairKey(i, Neighbor);
					if (DoorPairs.Contains(Key)) { bFoundDoor = true; break; }

					DoorPairs.Add(Key);

					FString WallDir;
					if (D.X > 0) WallDir = TEXT("east");
					else if (D.X < 0) WallDir = TEXT("west");
					else if (D.Y > 0) WallDir = TEXT("south");
					else WallDir = TEXT("north");

					FDoorDef Door;
					Door.DoorId = FString::Printf(TEXT("door_%02d"), ++DoorCounter);
					Door.RoomA = Rooms[i].RoomId;
					Door.RoomB = Rooms[Neighbor].RoomId;
					Door.EdgeStart = Cell;
					Door.EdgeEnd = FIntPoint(NX, NY);
					Door.Wall = WallDir;
					Door.Width = DoorWidth;
					Door.Height = 220.0f;
					Doors.Add(MoveTemp(Door));
					bFoundDoor = true;
					break;
				}
			}
			if (bFoundDoor) break;
		}
	}

	return Doors;
}

// ============================================================================
// Output JSON
// ============================================================================

TSharedPtr<FJsonObject> FMonolithMeshFloorPlanGenerator::BuildOutputJson(
	const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
	const TArray<FRoomDef>& Rooms, const TArray<FDoorDef>& Doors,
	const FString& ArchetypeName, float FootprintWidth, float FootprintHeight,
	bool bHospiceMode, float CellSize)
{
	auto Result = MakeShared<FJsonObject>();

	// Grid as 2D array
	TArray<TSharedPtr<FJsonValue>> GridArr;
	for (int32 Y = 0; Y < GridH; ++Y)
	{
		TArray<TSharedPtr<FJsonValue>> RowArr;
		for (int32 X = 0; X < GridW; ++X)
		{
			RowArr.Add(MakeShared<FJsonValueNumber>(Grid[Y][X]));
		}
		GridArr.Add(MakeShared<FJsonValueArray>(RowArr));
	}
	Result->SetArrayField(TEXT("grid"), GridArr);

	// Rooms
	TArray<TSharedPtr<FJsonValue>> RoomArr;
	for (const FRoomDef& R : Rooms)
	{
		RoomArr.Add(MakeShared<FJsonValueObject>(R.ToJson()));
	}
	Result->SetArrayField(TEXT("rooms"), RoomArr);

	// Doors
	TArray<TSharedPtr<FJsonValue>> DoorArr;
	for (const FDoorDef& D : Doors)
	{
		auto DJ = MakeShared<FJsonObject>();
		DJ->SetStringField(TEXT("door_id"), D.DoorId);
		DJ->SetStringField(TEXT("room_a"), D.RoomA);
		DJ->SetStringField(TEXT("room_b"), D.RoomB);

		TArray<TSharedPtr<FJsonValue>> EdgeStart, EdgeEnd;
		EdgeStart.Add(MakeShared<FJsonValueNumber>(D.EdgeStart.X));
		EdgeStart.Add(MakeShared<FJsonValueNumber>(D.EdgeStart.Y));
		EdgeEnd.Add(MakeShared<FJsonValueNumber>(D.EdgeEnd.X));
		EdgeEnd.Add(MakeShared<FJsonValueNumber>(D.EdgeEnd.Y));
		DJ->SetArrayField(TEXT("edge_start"), EdgeStart);
		DJ->SetArrayField(TEXT("edge_end"), EdgeEnd);

		DJ->SetStringField(TEXT("wall"), D.Wall);
		DJ->SetNumberField(TEXT("width"), D.Width);
		DJ->SetNumberField(TEXT("height"), D.Height);
		DoorArr.Add(MakeShared<FJsonValueObject>(DJ));
	}
	Result->SetArrayField(TEXT("doors"), DoorArr);

	// Metadata
	Result->SetStringField(TEXT("archetype"), ArchetypeName);

	auto Footprint = MakeShared<FJsonObject>();
	Footprint->SetNumberField(TEXT("width"), FootprintWidth);
	Footprint->SetNumberField(TEXT("height"), FootprintHeight);
	Result->SetObjectField(TEXT("footprint"), Footprint);

	Result->SetBoolField(TEXT("hospice_mode"), bHospiceMode);
	Result->SetNumberField(TEXT("cell_size"), CellSize);

	// Stats
	int32 CorridorCells = 0;
	for (const FRoomDef& R : Rooms)
	{
		if (R.RoomType == TEXT("corridor"))
			CorridorCells += R.GridCells.Num();
	}

	auto Stats = MakeShared<FJsonObject>();
	Stats->SetNumberField(TEXT("room_count"), Rooms.Num());
	Stats->SetNumberField(TEXT("door_count"), Doors.Num());
	Stats->SetNumberField(TEXT("corridor_cells"), CorridorCells);
	Stats->SetNumberField(TEXT("grid_width"), GridW);
	Stats->SetNumberField(TEXT("grid_height"), GridH);
	Stats->SetNumberField(TEXT("total_cells"), GridW * GridH);

	int32 UsedCells = 0;
	for (int32 Y = 0; Y < GridH; ++Y)
		for (int32 X = 0; X < GridW; ++X)
			if (Grid[Y][X] >= 0) ++UsedCells;
	Stats->SetNumberField(TEXT("used_cells"), UsedCells);
	Stats->SetNumberField(TEXT("fill_ratio"), (GridW * GridH > 0) ? static_cast<float>(UsedCells) / (GridW * GridH) : 0.0f);

	Result->SetObjectField(TEXT("stats"), Stats);

	return Result;
}

// ============================================================================
// Action handlers
// ============================================================================

FMonolithActionResult FMonolithMeshFloorPlanGenerator::GenerateFloorPlan(const TSharedPtr<FJsonObject>& Params)
{
	// ---- Parse params ----
	FString ArchetypeName;
	if (!Params->TryGetStringField(TEXT("archetype"), ArchetypeName) || ArchetypeName.IsEmpty())
		return FMonolithActionResult::Error(TEXT("Missing required 'archetype' parameter"));

	double FootprintW = 0, FootprintH = 0;
	if (!Params->TryGetNumberField(TEXT("footprint_width"), FootprintW) || FootprintW <= 0)
		return FMonolithActionResult::Error(TEXT("Missing or invalid 'footprint_width' (must be > 0)"));
	if (!Params->TryGetNumberField(TEXT("footprint_height"), FootprintH) || FootprintH <= 0)
		return FMonolithActionResult::Error(TEXT("Missing or invalid 'footprint_height' (must be > 0)"));

	double CellSizeDbl = 50.0;
	Params->TryGetNumberField(TEXT("cell_size"), CellSizeDbl);
	float CellSize = static_cast<float>(FMath::Max(10.0, CellSizeDbl));

	double SeedDbl = -1.0;
	Params->TryGetNumberField(TEXT("seed"), SeedDbl);
	int32 Seed = static_cast<int32>(SeedDbl);
	if (Seed < 0)
		Seed = FMath::Rand();

	bool bHospiceMode = false;
	Params->TryGetBoolField(TEXT("hospice_mode"), bHospiceMode);

	double MinAspectDbl = 3.0;
	Params->TryGetNumberField(TEXT("min_room_aspect"), MinAspectDbl);

	FRandomStream Rng(Seed);

	// ---- Parse optional floor_index for per-floor generation ----
	double FloorIndexDbl = -1.0;
	Params->TryGetNumberField(TEXT("floor_index"), FloorIndexDbl);
	int32 FloorIndex = static_cast<int32>(FloorIndexDbl);

	// ---- Load archetype ----
	FBuildingArchetype Archetype;
	FString LoadError;
	if (!LoadArchetype(ArchetypeName, Archetype, LoadError))
		return FMonolithActionResult::Error(LoadError);

	// ---- Validate stairwell requirement for multi-floor archetypes ----
	FString StairwellError = ValidateStairwellRequirement(Archetype);
	if (!StairwellError.IsEmpty())
		return FMonolithActionResult::Error(StairwellError);

	// ---- Compute grid dimensions ----
	int32 GridW = FMath::Max(2, FMath::RoundToInt32(static_cast<float>(FootprintW) / CellSize));
	int32 GridH = FMath::Max(2, FMath::RoundToInt32(static_cast<float>(FootprintH) / CellSize));

	// ---- Validate footprint capacity ----
	FString CapacityError = ValidateFootprintCapacity(Archetype, GridW, GridH, FloorIndex);
	if (!CapacityError.IsEmpty())
		return FMonolithActionResult::Error(CapacityError);

	UE_LOG(LogMonolithFloorPlan, Log, TEXT("Generating floor plan: archetype=%s, grid=%dx%d, cell=%.0f, seed=%d, hospice=%d, floor=%d"),
		*Archetype.Name, GridW, GridH, CellSize, Seed, bHospiceMode ? 1 : 0, FloorIndex);

	// ---- Resolve room instances (with per-floor filtering) ----
	TArray<FRoomInstance> Instances = ResolveRoomInstances(Archetype, GridW, GridH, Rng, FloorIndex);

	if (Instances.Num() == 0)
		return FMonolithActionResult::Error(TEXT("Archetype produced zero room instances for this floor. Check per-floor room assignments."));

	// ---- Squarified treemap layout ----
	TArray<FGridRect> Rects = SquarifiedTreemapLayout(Instances, GridW, GridH);

	// ---- Correct aspect ratios (per-room max_aspect from archetype) ----
	CorrectAspectRatios(Rects, Instances, GridW, GridH);

	// ---- Validate room quality (warn on remaining extreme aspect ratios) ----
	float MaxAspect = static_cast<float>(MinAspectDbl);
	for (int32 i = 0; i < Rects.Num(); ++i)
	{
		float RoomMaxAspect = (i < Instances.Num()) ? Instances[i].MaxAspect : MaxAspect;
		float EffectiveMax = FMath::Max(MaxAspect, RoomMaxAspect);
		if (Rects[i].AspectRatio() > EffectiveMax && Rects[i].Area() > 2)
		{
			UE_LOG(LogMonolithFloorPlan, Warning, TEXT("Room '%s' has extreme aspect ratio %.1f (%dx%d, max=%.1f) -- may produce awkward geometry"),
				*Instances[i].RoomId, Rects[i].AspectRatio(), Rects[i].W, Rects[i].H, EffectiveMax);
		}
	}

	// ---- Build grid ----
	TArray<TArray<int32>> Grid = BuildGridFromRects(Rects, GridW, GridH);

	// ---- Build initial room defs ----
	TArray<FRoomDef> Rooms = BuildRoomDefs(Grid, GridW, GridH, Instances);

	// ---- Insert corridors ----
	InsertCorridors(Grid, GridW, GridH, Rooms, Archetype.Adjacency, bHospiceMode, Rng);

	// ---- Hospice: rest alcoves ----
	if (bHospiceMode)
	{
		InsertRestAlcoves(Grid, GridW, GridH, Rooms, 4, Rng);
	}

	// ---- Place doors ----
	TArray<FDoorDef> Doors = PlaceDoors(Grid, GridW, GridH, Rooms, Archetype.Adjacency, bHospiceMode, Rng);

	// ---- Build output ----
	TSharedPtr<FJsonObject> ResultJson = BuildOutputJson(
		Grid, GridW, GridH, Rooms, Doors,
		Archetype.Name, static_cast<float>(FootprintW), static_cast<float>(FootprintH),
		bHospiceMode, CellSize);

	ResultJson->SetNumberField(TEXT("seed"), Seed);
	ResultJson->SetNumberField(TEXT("floor_index"), FloorIndex);
	ResultJson->SetNumberField(TEXT("floor_height"), Archetype.FloorHeight);

	// Include material hints if present
	if (!Archetype.MaterialHints.Exterior.IsEmpty() || !Archetype.MaterialHints.Interior.IsEmpty() || !Archetype.MaterialHints.FloorMaterial.IsEmpty())
	{
		auto MatHints = MakeShared<FJsonObject>();
		if (!Archetype.MaterialHints.Exterior.IsEmpty())
			MatHints->SetStringField(TEXT("exterior"), Archetype.MaterialHints.Exterior);
		if (!Archetype.MaterialHints.Interior.IsEmpty())
			MatHints->SetStringField(TEXT("interior"), Archetype.MaterialHints.Interior);
		if (!Archetype.MaterialHints.FloorMaterial.IsEmpty())
			MatHints->SetStringField(TEXT("floor"), Archetype.MaterialHints.FloorMaterial);
		ResultJson->SetObjectField(TEXT("material_hints"), MatHints);
	}

	UE_LOG(LogMonolithFloorPlan, Log, TEXT("Floor plan generated: %d rooms, %d doors, seed=%d, floor=%d"),
		Rooms.Num(), Doors.Num(), Seed, FloorIndex);

	return FMonolithActionResult::Success(ResultJson);
}

FMonolithActionResult FMonolithMeshFloorPlanGenerator::ListBuildingArchetypes(const TSharedPtr<FJsonObject>& Params)
{
	FString Dir = GetArchetypeDirectory();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);

	auto Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ArchetypeArr;

	for (const FString& File : Files)
	{
		FString Name = FPaths::GetBaseFilename(File);

		// Try to load and get the description
		FBuildingArchetype Arch;
		FString Err;
		FString FullPath = FPaths::Combine(Dir, File);

		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Name);
		Entry->SetStringField(TEXT("file"), FullPath);

		if (LoadArchetype(Name, Arch, Err))
		{
			Entry->SetStringField(TEXT("description"), Arch.Description);
			Entry->SetNumberField(TEXT("room_types"), Arch.Rooms.Num());
			Entry->SetNumberField(TEXT("adjacency_rules"), Arch.Adjacency.Num());
			Entry->SetStringField(TEXT("roof_type"), Arch.RoofType);

			auto Floors = MakeShared<FJsonObject>();
			Floors->SetNumberField(TEXT("min"), Arch.FloorsMin);
			Floors->SetNumberField(TEXT("max"), Arch.FloorsMax);
			Entry->SetObjectField(TEXT("floors"), Floors);
		}

		ArchetypeArr.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Result->SetArrayField(TEXT("archetypes"), ArchetypeArr);
	Result->SetNumberField(TEXT("count"), Files.Num());
	Result->SetStringField(TEXT("directory"), Dir);

	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithMeshFloorPlanGenerator::GetBuildingArchetype(const TSharedPtr<FJsonObject>& Params)
{
	FString ArchetypeName;
	if (!Params->TryGetStringField(TEXT("archetype"), ArchetypeName) || ArchetypeName.IsEmpty())
		return FMonolithActionResult::Error(TEXT("Missing required 'archetype' parameter"));

	// Load the raw JSON file
	FString FilePath;
	if (ArchetypeName.EndsWith(TEXT(".json")))
		FilePath = ArchetypeName;
	else
		FilePath = FPaths::Combine(GetArchetypeDirectory(), ArchetypeName + TEXT(".json"));

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
		return FMonolithActionResult::Error(FString::Printf(TEXT("Archetype not found: %s"), *FilePath));

	TSharedPtr<FJsonObject> Json = FMonolithJsonUtils::Parse(JsonString);
	if (!Json.IsValid())
		return FMonolithActionResult::Error(FString::Printf(TEXT("Failed to parse archetype JSON: %s"), *FilePath));

	// Return the raw JSON as the result, wrapped in a result object
	auto Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("archetype"), Json);
	Result->SetStringField(TEXT("file"), FilePath);

	return FMonolithActionResult::Success(Result);
}
