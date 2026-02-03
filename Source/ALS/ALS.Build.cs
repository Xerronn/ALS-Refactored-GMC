using UnrealBuildTool;

public class ALS : ModuleRules
{
	public ALS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Warning;

		// UnsafeTypeCastWarningLevel = WarningLevel.Warning;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", 
			"AnimGraphRuntime", "RigVM", "ControlRig", "GMCCore", "AIModule"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"EngineSettings", "NetCore", "PhysicsCore", "Niagara", "EnhancedInput"
		});

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"MessageLog"
			});
		}

		SetupIrisSupport(Target);
	}
}