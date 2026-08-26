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

public class AndroidVulkanVideoFactory : ModuleRules
{
	public AndroidVulkanVideoFactory(ReadOnlyTargetRules Target) : base(Target)
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
				"Projects"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject", "Engine","RHI","AudioExtensions"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
					"AndroidVulkanVideo",
					"Media",
					"MediaUtils"
			});



		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"Media"
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			DynamicallyLoadedModuleNames.Add("AndroidVulkanVideo");
		}
		// don't precompile video 
		// factory so any editor can use it
		bPrecompile = false;
		bUsePrecompiled = false;
	}
}
