// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Build AndroidVulkanVideo factory module (to make
// it possible to select this as a video source in 
// editor.)
// ------------------------------------------------

using UnrealBuildTool;
using System.IO;
using System;

public class VulkanVideoMeshComponent : ModuleRules
{
	public VulkanVideoMeshComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;


		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"CoreUObject", "Engine","RHI","MediaUtils"
				// ... add other public dependencies that you statically link with here ...
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicDependencyModuleNames.Add("VulkanRHI");
		}


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core","CoreUObject", "Engine","RHI","RenderCore","MediaUtils","Media","MediaAssets"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("VulkanRHI");
		}
		else
		{
			PrivateIncludePathModuleNames.Add("VulkanRHI");
		}


		PrivateIncludePathModuleNames.AddRange(
			new string[] {
					"AndroidVulkanVideo",
			});



		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			DynamicallyLoadedModuleNames.Add("AndroidVulkanVideo");
		}
	}
}
