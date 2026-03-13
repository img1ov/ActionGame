using UnrealBuildTool;

public class ActGame : ModuleRules
{
    public ActGame(ReadOnlyTargetRules Target) : base(Target)
    {
        // TODO:
        // When we create a new Gameplay module, by default, the new module will be automatically optimized, which causes the specific parameters will not be captured during IDE debugging.
        // So, change CodeOptimization to Default or delete this line when ActGame module finished.
        OptimizeCode = CodeOptimization.Never;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicIncludePaths.AddRange(
            new string[] {
                
            }
        );
        
        PrivateIncludePaths.AddRange(
            new string[] {
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreOnline",
                "CoreUObject",
                "ApplicationCore",
                "Engine",
                "PhysicsCore",
                "GameplayTags",
                "GameplayTasks",
                "GameplayAbilities",
                "AIModule",
                "ModularGameplay",
                "ModularGameplayActors",
                "DataRegistry",
                "ReplicationGraph",
                "GameFeatures",
                "SignificanceManager",
                "Hotfix",
                "Niagara",
                "ControlFlows",
                "PropertyPath",
                "StateTreeModule",
                "GameplayStateTreeModule",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "InputCore",
                "Slate",
                "SlateCore",
                "RenderCore",
                "DeveloperSettings",
                "EnhancedInput",
                "NetCore",
                "RHI",
                "Projects",
                "Gauntlet",
                "UMG",
                "CommonUI",
                "CommonInput",
                "AudioMixer",
                "NetworkReplayStreaming",
                "ClientPilot",
                "AudioModulation",
                "EngineSettings",
                "DTLSHandlerComponent",
                "Json",
            }
        );
    }
}
