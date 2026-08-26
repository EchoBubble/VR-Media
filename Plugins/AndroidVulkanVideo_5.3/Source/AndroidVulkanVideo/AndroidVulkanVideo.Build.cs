// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Build AndroidVulkanVideo module
// ------------------------------------------------

using UnrealBuildTool;
using System.IO;
using System;

public class AndroidVulkanVideo : ModuleRules
{

	public AndroidVulkanVideo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;

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
				"CoreUObject", "Engine","RHI","VulkanRHI","AudioExtensions","MediaUtils","MediaAssets"
				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"Media"
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
					"Media","MediaUtils"
			});

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string pluginDir = Path.GetFullPath(PluginDirectory);
			string upluginPath = Path.Combine(pluginDir, "AndroidVulkanVideo.uplugin");
			string pluginVersion = null;
			// do this using the uplugin file directly, as older versions of unreal don't support
			// getting installed plugins
			if (File.Exists(upluginPath))
			{
				var upluginJson = File.ReadAllText(upluginPath);
				var match = System.Text.RegularExpressions.Regex.Match(upluginJson, "\"Version\".*:\\D*(\\d+)");
				if (match.Success)
				{
					pluginVersion = match.Groups[1].Value;
				}
			}
			if(pluginVersion == null){
				throw new InvalidOperationException("Could not find plugin version in AndroidVulkanVideo.uplugin");
			}
			PublicDefinitions.Add(string.Format("DIRECTVIDEO_VERSION_CODE=\"{0}\"", pluginVersion));

			PrivateDependencyModuleNames.AddRange(new string[] { "Launch" });
			// AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "Copy_Lib_Android.xml"));
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
			PublicSystemLibraries.Add("libmediandk");
			string overridelib_src_path = Path.GetFullPath("../overridelib/BuildOverrideLib.xml", ModuleDirectory);
			string overridelib_built_path = Path.GetFullPath("./overridelib_built/InstallOverrideLib.xml",ModuleDirectory);
			if(File.Exists(overridelib_src_path)){
				// build vulkan override layer from scratch if we have code
				// check that the correct versioncode is in the plugin
				// Read the XML file and replace versionCode value with pluginVersion
				var xmlDoc = new System.Xml.XmlDocument();
				xmlDoc.Load(overridelib_src_path);
				var nodes = xmlDoc.SelectNodes("//setString[@result='versionCode']");
				bool changed = false;
				foreach (System.Xml.XmlNode node in nodes)
				{
					var attr = node.Attributes["value"];
					if (attr != null && attr.Value != pluginVersion)
					{
						attr.Value = pluginVersion;
						changed = true;
					}
				}
				if (changed)
				{
					xmlDoc.Save(overridelib_src_path);
				}

				AdditionalPropertiesForReceipt.Add("AndroidPlugin", overridelib_src_path);
			}else if(File.Exists(overridelib_built_path)){
				// otherwise copy pre-built vulkan override layer .so into project
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", overridelib_built_path);
			}else{
				// ERROR
				throw new InvalidOperationException("Need "+overridelib_built_path+" to build");
			}
		}

		bPrecompile	= true;	

	}
}
