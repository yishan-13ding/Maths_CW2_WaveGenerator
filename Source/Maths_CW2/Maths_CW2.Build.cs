// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Maths_CW2 : ModuleRules
{
	public Maths_CW2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        //PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

        PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "SignalProcessing",     // 检查拼写是否完全一致
    "ProceduralMeshComponent"
});

        PrivateDependencyModuleNames.AddRange(new string[] {  });

        //AddThirdPartyPrivateStaticDependencies(Target, "Kiss_FFT");

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
