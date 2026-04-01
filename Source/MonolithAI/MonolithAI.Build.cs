using UnrealBuildTool;
using System.IO;

public class MonolithAI : ModuleRules
{
	public MonolithAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Always-available engine AI modules
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine",
			"AIModule", "GameplayTasks", "GameplayTags", "NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MonolithCore", "MonolithBlueprint", "MonolithIndex",
			"UnrealEd", "BlueprintGraph", "AIGraph",
			"BehaviorTreeEditor", "EnvironmentQueryEditor",
			"GameplayStateTreeModule",
			"StateTreeModule", "StateTreeEditorModule", "PropertyBindingUtils", "StructUtils",
			"SmartObjectsModule", "SmartObjectsEditorModule",
			"Json", "JsonUtilities",
			"SQLiteCore"
		});

		// StateTree and SmartObjects are required deps — always define these
		PublicDefinitions.Add("WITH_STATETREE=1");
		PublicDefinitions.Add("WITH_SMARTOBJECTS=1");

		// --- Conditional optional deps ---
		bool bReleaseBuild = System.Environment.GetEnvironmentVariable("MONOLITH_RELEASE_BUILD") == "1";

		// Hoist engine paths — shared across all conditional probes
		string EngineDir = "";
		string EnginePluginsDir = "";
		if (!bReleaseBuild)
		{
			EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
			EnginePluginsDir = Path.Combine(EngineDir, "Plugins");
		}

		// --- Conditional: GameplayBehaviors (Experimental) ---
		bool bHasGameplayBehaviors = false;
		if (!bReleaseBuild)
		{
			if (Directory.Exists(Path.Combine(EnginePluginsDir, "GameplayBehaviors")))
			{
				bHasGameplayBehaviors = true;
			}
			else
			{
				string[] SearchDirs = new string[]
				{
					Path.Combine(EnginePluginsDir, "Runtime"),
					Path.Combine(EnginePluginsDir, "Experimental"),
					Path.Combine(EnginePluginsDir, "AI")
				};
				foreach (string Dir in SearchDirs)
				{
					if (Directory.Exists(Dir) &&
						Directory.GetDirectories(Dir, "GameplayBehaviors*", SearchOption.TopDirectoryOnly).Length > 0)
					{
						bHasGameplayBehaviors = true;
						break;
					}
				}
			}
		}

		if (bHasGameplayBehaviors)
		{
			PrivateDependencyModuleNames.Add("GameplayBehaviorsModule");
			PublicDefinitions.Add("WITH_GAMEPLAYBEHAVIORS=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMEPLAYBEHAVIORS=0");
		}

		// --- Conditional: MassEntity (Experimental) ---
		bool bHasMassEntity = false;
		if (!bReleaseBuild)
		{
			string[] MassSearchDirs = new string[]
			{
				EnginePluginsDir,
				Path.Combine(EnginePluginsDir, "Runtime"),
				Path.Combine(EnginePluginsDir, "AI")
			};
			foreach (string Dir in MassSearchDirs)
			{
				if (Directory.Exists(Dir) &&
					Directory.GetDirectories(Dir, "MassEntity*", SearchOption.TopDirectoryOnly).Length > 0)
				{
					bHasMassEntity = true;
					break;
				}
			}
		}

		if (bHasMassEntity)
		{
			// MassGameplayEditor lives in Runtime/MassGameplay — assumed co-installed with MassEntity
			PrivateDependencyModuleNames.AddRange(new string[] { "MassEntity", "MassSpawner", "MassGameplayEditor" });
			PublicDefinitions.Add("WITH_MASSENTITY=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_MASSENTITY=0");
		}

		// --- Conditional: ZoneGraph (Experimental) ---
		bool bHasZoneGraph = false;
		if (!bReleaseBuild)
		{
			string[] ZoneSearchDirs = new string[]
			{
				EnginePluginsDir,
				Path.Combine(EnginePluginsDir, "Runtime"),
				Path.Combine(EnginePluginsDir, "Experimental")
			};
			foreach (string Dir in ZoneSearchDirs)
			{
				if (Directory.Exists(Dir) &&
					Directory.GetDirectories(Dir, "ZoneGraph*", SearchOption.TopDirectoryOnly).Length > 0)
				{
					bHasZoneGraph = true;
					break;
				}
			}
		}

		if (bHasZoneGraph)
		{
			PrivateDependencyModuleNames.Add("ZoneGraph");
			PublicDefinitions.Add("WITH_ZONEGRAPH=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ZONEGRAPH=0");
		}
	}
}
