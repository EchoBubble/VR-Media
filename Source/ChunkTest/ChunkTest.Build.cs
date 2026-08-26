// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ChunkTest : ModuleRules
{
	public ChunkTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" , "WebSockets","Json", "MediaAssets","HTTP"});

		PrivateDependencyModuleNames.AddRange(new string[] {  "Sockets", "Networking","ChunkDownloader" });

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
        
			string PluginPath = Utils.MakePathRelativeTo(
				ModuleDirectory,
				Target.RelativeEnginePath
			);
        
			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				System.IO.Path.Combine(PluginPath, "VR_Midea_UPL.xml")
			);
		}
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
