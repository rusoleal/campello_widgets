#!/usr/bin/env python3
"""
Generate a minimal FidelityReference.xcodeproj for an iOS UIKit app.
Run this after adding/removing Swift source files.
"""

import hashlib
import json
import os
import uuid

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.join(PROJECT_DIR, "FidelityReference")
XCODEPROJ_DIR = os.path.join(PROJECT_DIR, "FidelityReference.xcodeproj")
PBXPROJ_PATH = os.path.join(XCODEPROJ_DIR, "project.pbxproj")

SWIFT_SOURCES = sorted(
    os.path.join("FidelityReference", f)
    for f in os.listdir(SOURCE_DIR)
    if f.endswith(".swift") and os.path.isfile(os.path.join(SOURCE_DIR, f))
)


def gid(seed: str) -> str:
    """Deterministic 24-char Xcode-style GUID."""
    h = hashlib.md5(seed.encode()).hexdigest().upper()
    return h[:8] + h[8:16] + h[16:24]


# Stable identifiers
PROJECT_GID = gid("project")
MAIN_GROUP_GID = gid("mainGroup")
TARGET_GID = gid("target")
TARGET_BUILD_CONFIG_LIST_GID = gid("targetBuildConfigList")
PROJECT_BUILD_CONFIG_LIST_GID = gid("projectBuildConfigList")
SOURCES_BUILD_PHASE_GID = gid("sourcesBuildPhase")
FRAMEWORKS_BUILD_PHASE_GID = gid("frameworksBuildPhase")
RESOURCES_BUILD_PHASE_GID = gid("resourcesBuildPhase")


# File references + build files
file_refs = []
build_files = []
for src in SWIFT_SOURCES:
    seed = f"file:{src}"
    file_ref_gid = gid(seed)
    build_file_gid = gid(f"build:{src}")
    file_refs.append((src, file_ref_gid))
    build_files.append((src, build_file_gid, file_ref_gid))


def indent(level: int, text: str) -> str:
    return "\t" * level + text


def pbxproj_content() -> str:
    lines = [
        "// !$*UTF8*$!",
        "{",
        indent(1, "archiveVersion = 1;"),
        indent(1, "classes = {"),
        indent(1, "};"),
        indent(1, "objectVersion = 56;"),
        indent(1, "objects = {"),
    ]

    # PBXBuildFile
    lines.append(indent(2, "/* Begin PBXBuildFile section */"))
    for _, build_file_gid, file_ref_gid in build_files:
        lines.append(indent(2, f"{build_file_gid} /* in Sources */ = {{isa = PBXBuildFile; fileRef = {file_ref_gid}; }};"))
    lines.append(indent(2, "/* End PBXBuildFile section */"))
    lines.append("")

    # PBXFileReference
    lines.append(indent(2, "/* Begin PBXFileReference section */"))
    for src, file_ref_gid in file_refs:
        lines.append(indent(2, f"{file_ref_gid} /* {os.path.basename(src)} */ = {{isa = PBXFileReference; lastKnownFileType = sourcecode.swift; path = {src}; sourceTree = SOURCE_ROOT; }};"))
    # Product reference
    product_gid = gid("product")
    lines.append(indent(2, f"{product_gid} /* FidelityReference.app */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; includeInIndex = 0; path = FidelityReference.app; sourceTree = BUILT_PRODUCTS_DIR; }};"))
    lines.append(indent(2, "/* End PBXFileReference section */"))
    lines.append("")

    # PBXFrameworksBuildPhase
    lines.append(indent(2, "/* Begin PBXFrameworksBuildPhase section */"))
    lines.append(indent(2, f"{FRAMEWORKS_BUILD_PHASE_GID} /* Frameworks */ = {{"))
    lines.append(indent(3, "isa = PBXFrameworksBuildPhase;"))
    lines.append(indent(3, "buildActionMask = 2147483647;"))
    lines.append(indent(3, "files = ("))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "runOnlyForDeploymentPostprocessing = 0;"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End PBXFrameworksBuildPhase section */"))
    lines.append("")

    # PBXGroup
    lines.append(indent(2, "/* Begin PBXGroup section */"))
    # Main group children
    main_children = [f"{file_ref_gid} /* {os.path.basename(src)} */" for src, file_ref_gid in file_refs]
    main_children.append(f"{product_gid} /* FidelityReference.app */")
    lines.append(indent(2, f"{MAIN_GROUP_GID} = {{"))
    lines.append(indent(3, "isa = PBXGroup;"))
    lines.append(indent(3, "children = ("))
    for c in main_children:
        lines.append(indent(4, c + ","))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "sourceTree = \"<group>\";"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End PBXGroup section */"))
    lines.append("")

    # PBXNativeTarget
    lines.append(indent(2, "/* Begin PBXNativeTarget section */"))
    lines.append(indent(2, f"{TARGET_GID} /* FidelityReference */ = {{"))
    lines.append(indent(3, "isa = PBXNativeTarget;"))
    lines.append(indent(3, "buildConfigurationList = " + TARGET_BUILD_CONFIG_LIST_GID + " /* Build configuration list for PBXNativeTarget \"FidelityReference\" */;"))
    lines.append(indent(3, "buildPhases = ("))
    lines.append(indent(4, f"{SOURCES_BUILD_PHASE_GID} /* Sources */,"))
    lines.append(indent(4, f"{FRAMEWORKS_BUILD_PHASE_GID} /* Frameworks */,"))
    lines.append(indent(4, f"{RESOURCES_BUILD_PHASE_GID} /* Resources */,"))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "buildRules = ("))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "dependencies = ("))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "name = FidelityReference;"))
    lines.append(indent(3, "productName = FidelityReference;"))
    lines.append(indent(3, f"productReference = {product_gid} /* FidelityReference.app */;"))
    lines.append(indent(3, "productType = \"com.apple.product-type.application\";"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End PBXNativeTarget section */"))
    lines.append("")

    # PBXProject
    lines.append(indent(2, "/* Begin PBXProject section */"))
    lines.append(indent(2, f"{PROJECT_GID} /* Project object */ = {{"))
    lines.append(indent(3, "isa = PBXProject;"))
    lines.append(indent(3, "buildConfigurationList = " + PROJECT_BUILD_CONFIG_LIST_GID + " /* Build configuration list for PBXProject \"FidelityReference\" */;"))
    lines.append(indent(3, "compatibilityVersion = \"Xcode 14.0\";"))
    lines.append(indent(3, "developmentRegion = en;"))
    lines.append(indent(3, "hasScannedForEncodings = 0;"))
    lines.append(indent(3, "knownRegions = ("))
    lines.append(indent(4, "en,"))
    lines.append(indent(4, "Base,"))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "mainGroup = " + MAIN_GROUP_GID + ";"))
    lines.append(indent(3, "productRefGroup = " + MAIN_GROUP_GID + ";"))
    lines.append(indent(3, "projectDirPath = \"\";"))
    lines.append(indent(3, "projectRoot = \"\";"))
    lines.append(indent(3, "targets = ("))
    lines.append(indent(4, TARGET_GID + " /* FidelityReference */,"))
    lines.append(indent(3, ");"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End PBXProject section */"))
    lines.append("")

    # PBXResourcesBuildPhase
    lines.append(indent(2, "/* Begin PBXResourcesBuildPhase section */"))
    lines.append(indent(2, f"{RESOURCES_BUILD_PHASE_GID} /* Resources */ = {{"))
    lines.append(indent(3, "isa = PBXResourcesBuildPhase;"))
    lines.append(indent(3, "buildActionMask = 2147483647;"))
    lines.append(indent(3, "files = ("))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "runOnlyForDeploymentPostprocessing = 0;"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End PBXResourcesBuildPhase section */"))
    lines.append("")

    # PBXSourcesBuildPhase
    lines.append(indent(2, "/* Begin PBXSourcesBuildPhase section */"))
    lines.append(indent(2, f"{SOURCES_BUILD_PHASE_GID} /* Sources */ = {{"))
    lines.append(indent(3, "isa = PBXSourcesBuildPhase;"))
    lines.append(indent(3, "buildActionMask = 2147483647;"))
    lines.append(indent(3, "files = ("))
    for src, build_file_gid, _ in build_files:
        lines.append(indent(4, f"{build_file_gid} /* {os.path.basename(src)} in Sources */,"))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "runOnlyForDeploymentPostprocessing = 0;"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End PBXSourcesBuildPhase section */"))
    lines.append("")

    # XCBuildConfiguration
    lines.append(indent(2, "/* Begin XCBuildConfiguration section */"))

    # Debug project
    debug_project_gid = gid("debug:project")
    lines.append(indent(2, f"{debug_project_gid} /* Debug */ = {{"))
    lines.append(indent(3, "isa = XCBuildConfiguration;"))
    lines.append(indent(3, "buildSettings = {"))
    lines.append(indent(4, "ALWAYS_SEARCH_USER_PATHS = NO;"))
    lines.append(indent(4, "CLANG_ANALYZER_NONNULL = YES;"))
    lines.append(indent(4, "CLANG_CXX_LANGUAGE_STANDARD = \"gnu++20\";"))
    lines.append(indent(4, "CLANG_ENABLE_MODULES = YES;"))
    lines.append(indent(4, "COPY_PHASE_STRIP = NO;"))
    lines.append(indent(4, "ENABLE_USER_SCRIPT_SANDBOXING = NO;"))
    lines.append(indent(4, "IPHONEOS_DEPLOYMENT_TARGET = 16.0;"))
    lines.append(indent(4, "MTL_ENABLE_DEBUG_INFO = INCLUDE_AND_ENABLE;"))
    lines.append(indent(4, "ONLY_ACTIVE_ARCH = YES;"))
    lines.append(indent(4, "SDKROOT = iphoneos;"))
    lines.append(indent(4, "SWIFT_ACTIVE_COMPILATION_CONDITIONS = \"DEBUG\";"))
    lines.append(indent(4, "SWIFT_OPTIMIZATION_LEVEL = \"-Onone\";"))
    lines.append(indent(3, "};"))
    lines.append(indent(3, "name = Debug;"))
    lines.append(indent(2, "};"))

    # Release project
    release_project_gid = gid("release:project")
    lines.append(indent(2, f"{release_project_gid} /* Release */ = {{"))
    lines.append(indent(3, "isa = XCBuildConfiguration;"))
    lines.append(indent(3, "buildSettings = {"))
    lines.append(indent(4, "ALWAYS_SEARCH_USER_PATHS = NO;"))
    lines.append(indent(4, "CLANG_ANALYZER_NONNULL = YES;"))
    lines.append(indent(4, "CLANG_CXX_LANGUAGE_STANDARD = \"gnu++20\";"))
    lines.append(indent(4, "CLANG_ENABLE_MODULES = YES;"))
    lines.append(indent(4, "COPY_PHASE_STRIP = NO;"))
    lines.append(indent(4, "ENABLE_USER_SCRIPT_SANDBOXING = NO;"))
    lines.append(indent(4, "IPHONEOS_DEPLOYMENT_TARGET = 16.0;"))
    lines.append(indent(4, "MTL_ENABLE_DEBUG_INFO = NO;"))
    lines.append(indent(4, "SDKROOT = iphoneos;"))
    lines.append(indent(4, "SWIFT_OPTIMIZATION_LEVEL = \"-O\";"))
    lines.append(indent(3, "};"))
    lines.append(indent(3, "name = Release;"))
    lines.append(indent(2, "};"))

    # Debug target
    debug_target_gid = gid("debug:target")
    lines.append(indent(2, f"{debug_target_gid} /* Debug */ = {{"))
    lines.append(indent(3, "isa = XCBuildConfiguration;"))
    lines.append(indent(3, "buildSettings = {"))
    lines.append(indent(4, "ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon;"))
    lines.append(indent(4, "CODE_SIGN_STYLE = Automatic;"))
    lines.append(indent(4, "CURRENT_PROJECT_VERSION = 1;"))
    lines.append(indent(4, "DEVELOPMENT_TEAM = \"\";"))
    lines.append(indent(4, "GENERATE_INFOPLIST_FILE = YES;"))
    lines.append(indent(4, "INFOPLIST_KEY_UIApplicationSceneManifest_Generation = NO;"))
    lines.append(indent(4, "INFOPLIST_KEY_UIApplicationSupportsIndirectInputEvents = YES;"))
    lines.append(indent(4, "INFOPLIST_KEY_UILaunchScreen_Generation = YES;"))
    lines.append(indent(4, "INFOPLIST_KEY_UISupportedInterfaceOrientations_iPad = \"UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight\";"))
    lines.append(indent(4, "INFOPLIST_KEY_UISupportedInterfaceOrientations_iPhone = \"UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight\";"))
    lines.append(indent(4, "IPHONEOS_DEPLOYMENT_TARGET = 16.0;"))
    lines.append(indent(4, "LD_RUNPATH_SEARCH_PATHS = (\"@executable_path/Frameworks\");"))
    lines.append(indent(4, "MARKETING_VERSION = 1.0;"))
    lines.append(indent(4, "PRODUCT_BUNDLE_IDENTIFIER = systems.leal.FidelityReference;"))
    lines.append(indent(4, "PRODUCT_NAME = \"$(TARGET_NAME)\";"))
    lines.append(indent(4, "SWIFT_EMIT_LOC_STRINGS = YES;"))
    lines.append(indent(4, "SWIFT_VERSION = 6.0;"))
    lines.append(indent(4, "TARGETED_DEVICE_FAMILY = \"1,2\";"))
    lines.append(indent(3, "};"))
    lines.append(indent(3, "name = Debug;"))
    lines.append(indent(2, "};"))

    # Release target
    release_target_gid = gid("release:target")
    lines.append(indent(2, f"{release_target_gid} /* Release */ = {{"))
    lines.append(indent(3, "isa = XCBuildConfiguration;"))
    lines.append(indent(3, "buildSettings = {"))
    lines.append(indent(4, "ASSETCATALOG_COMPILER_APPICON_NAME = AppIcon;"))
    lines.append(indent(4, "CODE_SIGN_STYLE = Automatic;"))
    lines.append(indent(4, "CURRENT_PROJECT_VERSION = 1;"))
    lines.append(indent(4, "DEVELOPMENT_TEAM = \"\";"))
    lines.append(indent(4, "GENERATE_INFOPLIST_FILE = YES;"))
    lines.append(indent(4, "INFOPLIST_KEY_UIApplicationSceneManifest_Generation = NO;"))
    lines.append(indent(4, "INFOPLIST_KEY_UIApplicationSupportsIndirectInputEvents = YES;"))
    lines.append(indent(4, "INFOPLIST_KEY_UILaunchScreen_Generation = YES;"))
    lines.append(indent(4, "INFOPLIST_KEY_UISupportedInterfaceOrientations_iPad = \"UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight\";"))
    lines.append(indent(4, "INFOPLIST_KEY_UISupportedInterfaceOrientations_iPhone = \"UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight\";"))
    lines.append(indent(4, "IPHONEOS_DEPLOYMENT_TARGET = 16.0;"))
    lines.append(indent(4, "LD_RUNPATH_SEARCH_PATHS = (\"@executable_path/Frameworks\");"))
    lines.append(indent(4, "MARKETING_VERSION = 1.0;"))
    lines.append(indent(4, "PRODUCT_BUNDLE_IDENTIFIER = systems.leal.FidelityReference;"))
    lines.append(indent(4, "PRODUCT_NAME = \"$(TARGET_NAME)\";"))
    lines.append(indent(4, "SWIFT_EMIT_LOC_STRINGS = YES;"))
    lines.append(indent(4, "SWIFT_VERSION = 6.0;"))
    lines.append(indent(4, "TARGETED_DEVICE_FAMILY = \"1,2\";"))
    lines.append(indent(3, "};"))
    lines.append(indent(3, "name = Release;"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End XCBuildConfiguration section */"))
    lines.append("")

    # XCConfigurationList
    lines.append(indent(2, "/* Begin XCConfigurationList section */"))
    lines.append(indent(2, f"{PROJECT_BUILD_CONFIG_LIST_GID} /* Build configuration list for PBXProject \"FidelityReference\" */ = {{"))
    lines.append(indent(3, "isa = XCConfigurationList;"))
    lines.append(indent(3, "buildConfigurations = ("))
    lines.append(indent(4, debug_project_gid + " /* Debug */,"))
    lines.append(indent(4, release_project_gid + " /* Release */,"))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "defaultConfigurationIsVisible = 0;"))
    lines.append(indent(3, "defaultConfigurationName = Release;"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, f"{TARGET_BUILD_CONFIG_LIST_GID} /* Build configuration list for PBXNativeTarget \"FidelityReference\" */ = {{"))
    lines.append(indent(3, "isa = XCConfigurationList;"))
    lines.append(indent(3, "buildConfigurations = ("))
    lines.append(indent(4, debug_target_gid + " /* Debug */,"))
    lines.append(indent(4, release_target_gid + " /* Release */,"))
    lines.append(indent(3, ");"))
    lines.append(indent(3, "defaultConfigurationIsVisible = 0;"))
    lines.append(indent(3, "defaultConfigurationName = Release;"))
    lines.append(indent(2, "};"))
    lines.append(indent(2, "/* End XCConfigurationList section */"))
    lines.append("")

    lines.extend([
        indent(1, "};"),
        indent(1, "rootObject = " + PROJECT_GID + " /* Project object */;"),
        "}",
    ])
    return "\n".join(lines) + "\n"


def main():
    os.makedirs(XCODEPROJ_DIR, exist_ok=True)
    content = pbxproj_content()
    with open(PBXPROJ_PATH, "w") as f:
        f.write(content)
    print(f"Generated {PBXPROJ_PATH}")
    print(f"Sources: {SWIFT_SOURCES}")


if __name__ == "__main__":
    main()
