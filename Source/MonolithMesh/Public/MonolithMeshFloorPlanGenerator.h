#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"
#include "MonolithMeshBuildingTypes.h"

/**
 * SP2: Automatic Floor Plan Generator
 *
 * Given a building archetype + footprint dimensions, generates a grid + rooms + doors
 * that feed directly into SP1's create_building_from_grid.
 *
 * Algorithm: Graph-based topology -> Squarified treemap layout -> Corridor insertion -> Door placement
 *
 * 3 actions:
 *   - generate_floor_plan: Full pipeline from archetype to grid/rooms/doors
 *   - list_building_archetypes: List available archetype JSON files
 *   - get_building_archetype: Return a specific archetype's JSON definition
 *
 * No GeometryScript dependency -- this is pure layout math that outputs data for SP1.
 */
class FMonolithMeshFloorPlanGenerator
{
public:
	static void RegisterActions(FMonolithToolRegistry& Registry);

private:
	// ---- Action handlers ----
	static FMonolithActionResult GenerateFloorPlan(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult ListBuildingArchetypes(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult GetBuildingArchetype(const TSharedPtr<FJsonObject>& Params);

	// ---- Archetype types ----

	/** A room type definition from an archetype */
	struct FArchetypeRoom
	{
		FString Type;
		float MinArea = 10.0f;
		float MaxArea = 30.0f;
		int32 CountMin = 1;
		int32 CountMax = 1;
		bool bRequired = true;
		int32 Priority = 5;
		bool bAutoGenerate = false;   // For corridor type
		bool bExteriorWall = false;   // Prefers exterior placement

		// Per-floor assignment: "ground", "upper", "every", "any" (default)
		FString Floor = TEXT("any");

		// Aspect ratio constraints
		float MinAspect = 1.0f;       // Minimum width/height ratio (e.g. 1.0 = square OK)
		float MaxAspect = 3.0f;       // Maximum (prevents 1x20 rooms)
	};

	/** An adjacency constraint from an archetype */
	struct FAdjacencyRule
	{
		FString From;
		FString To;
		FString Strength;  // "required", "strong", "preferred", "weak"
	};

	/** Material hints for future material assignment */
	struct FMaterialHints
	{
		FString Exterior;
		FString Interior;
		FString FloorMaterial;   // "Floor" conflicts with floor index
	};

	/** A complete loaded archetype */
	struct FBuildingArchetype
	{
		FString Name;
		FString Description;
		TArray<FArchetypeRoom> Rooms;
		TArray<FAdjacencyRule> Adjacency;
		int32 FloorsMin = 1;
		int32 FloorsMax = 1;
		FString RoofType;
		float FloorHeight = 270.0f;      // Default floor height in cm
		FMaterialHints MaterialHints;
	};

	/** A resolved room instance (after rolling counts from archetype ranges) */
	struct FRoomInstance
	{
		FString RoomId;       // e.g. "bedroom_1", "kitchen"
		FString RoomType;     // e.g. "bedroom", "kitchen"
		float TargetArea;     // In grid cells
		int32 Priority;
		bool bExteriorWall;
		FString Floor;        // "ground", "upper", "every", "any"
		float MinAspect = 1.0f;
		float MaxAspect = 3.0f;
	};

	/** A rectangle in grid space produced by treemap layout */
	struct FGridRect
	{
		int32 X = 0;
		int32 Y = 0;
		int32 W = 0;
		int32 H = 0;

		int32 Area() const { return W * H; }
		float AspectRatio() const { return (W > 0 && H > 0) ? FMath::Max((float)W / H, (float)H / W) : 999.0f; }
	};

	// ---- File I/O ----

	/** Get the archetype directory path */
	static FString GetArchetypeDirectory();

	/** Load an archetype from a JSON file */
	static bool LoadArchetype(const FString& ArchetypeName, FBuildingArchetype& OutArchetype, FString& OutError);

	/** Parse a JSON object into an archetype struct */
	static bool ParseArchetypeJson(const TSharedPtr<FJsonObject>& Json, FBuildingArchetype& OutArchetype, FString& OutError);

	// ---- Room resolution ----

	/** Resolve archetype room definitions into concrete room instances using a seed.
	 *  FloorIndex controls per-floor filtering: 0 = ground, 1+ = upper, -1 = all floors (legacy behavior). */
	static TArray<FRoomInstance> ResolveRoomInstances(const FBuildingArchetype& Archetype, int32 GridW, int32 GridH, FRandomStream& Rng, int32 FloorIndex = -1);

	/** Validate that the footprint can fit all required rooms. Returns empty string on success, error message on failure. */
	static FString ValidateFootprintCapacity(const FBuildingArchetype& Archetype, int32 GridW, int32 GridH, int32 FloorIndex = -1);

	/** Validate that multi-floor archetypes have stairwell entries. Returns empty string on success, error message on failure. */
	static FString ValidateStairwellRequirement(const FBuildingArchetype& Archetype);

	/** Post-layout aspect ratio correction: tries to reshape rooms that exceed their max_aspect */
	static void CorrectAspectRatios(TArray<FGridRect>& Rects, const TArray<FRoomInstance>& Rooms, int32 GridW, int32 GridH);

	// ---- Squarified treemap ----

	/** Run squarified treemap layout to pack rooms into the footprint */
	static TArray<FGridRect> SquarifiedTreemapLayout(TArray<FRoomInstance>& Rooms, int32 GridW, int32 GridH);

	/** Internal: layout a single row of rooms within a rectangle */
	static void LayoutRow(const TArray<int32>& RowIndices, const TArray<float>& Areas, FGridRect& Rect,
		bool bHorizontal, TArray<FGridRect>& OutRects);

	/** Calculate the worst aspect ratio if we lay out the given areas in a row against the given length */
	static float WorstAspectRatio(const TArray<float>& RowAreas, float SideLength);

	// ---- Corridor insertion ----

	/** Insert corridor cells where rooms need connectivity but don't share edges */
	static void InsertCorridors(TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		TArray<FRoomDef>& Rooms, const TArray<FAdjacencyRule>& Adjacency,
		bool bHospiceMode, FRandomStream& Rng);

	/** Check if two rooms share at least one grid edge */
	static bool RoomsShareEdge(const FRoomDef& A, const FRoomDef& B);

	/** Find a path between two rooms through empty or existing corridor cells using BFS */
	static TArray<FIntPoint> FindCorridorPath(const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		const FRoomDef& From, const FRoomDef& To, int32 CorridorRoomIndex, int32 CorridorWidth);

	// ---- Door placement ----

	/** Place doors at room boundaries based on adjacency graph */
	static TArray<FDoorDef> PlaceDoors(const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		const TArray<FRoomDef>& Rooms, const TArray<FAdjacencyRule>& Adjacency,
		bool bHospiceMode, FRandomStream& Rng);

	/** Find shared edge cells between two rooms */
	static TArray<FIntPoint> FindSharedEdge(const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		int32 RoomIndexA, int32 RoomIndexB);

	// ---- Hospice mode ----

	/** Insert rest alcoves per hospice requirements */
	static void InsertRestAlcoves(TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		TArray<FRoomDef>& Rooms, int32 RoomsBetweenAlcoves, FRandomStream& Rng);

	// ---- Grid utilities ----

	/** Convert room rects to a populated 2D grid */
	static TArray<TArray<int32>> BuildGridFromRects(const TArray<FGridRect>& Rects, int32 GridW, int32 GridH);

	/** Build FRoomDef array from grid state */
	static TArray<FRoomDef> BuildRoomDefs(const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		const TArray<FRoomInstance>& Instances);

	/** Convert grid + rooms + doors to the JSON output format compatible with create_building_from_grid */
	static TSharedPtr<FJsonObject> BuildOutputJson(const TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
		const TArray<FRoomDef>& Rooms, const TArray<FDoorDef>& Doors,
		const FString& ArchetypeName, float FootprintWidth, float FootprintHeight,
		bool bHospiceMode, float CellSize);
};
