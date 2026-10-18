// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyProject1 : ModuleRules
{
	public MyProject1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore" });

		PublicIncludePaths.AddRange(new string[] {
			"MyProject1",
			"MyProject1/Variant_Horror",
			"MyProject1/Variant_Horror/UI",
			"MyProject1/Variant_Shooter",
			"MyProject1/Variant_Shooter/AI",
			"MyProject1/Variant_Shooter/UI",
			"MyProject1/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
