# SPDX-License-Identifier: BSD-3-Clause
"""
Valecium OS Build System

Main build configuration file using SCons.
"""

import os
import shutil
import subprocess
from pathlib import Path

from SCons.Environment import Environment
from SCons.Variables import EnumVariable, Variables

from scripts.scons.arch import GetArchConfig, GetSupportedArchitectures
from scripts.scons.bootloader import GetSupportedBootTypes
from scripts.scons.kernel import GetSupportedKernelTypes
from scripts.scons.utility import GetGitHash, ResolveTools


ConfigPath = Path(".config.py")
if not ConfigPath.exists():
    DefaultConfig = {
        "BuildConfig": "debug",
        "BuildArch": "i686",
        "ProjVersion": "0.29",
        "BuildType": "full",
        "KernelName": "valeciumx",
        "KernelType": "generic",
        "BootType": "bios",
        "BootSystem": "grub",
    }
    with open(ConfigPath, "w", encoding="utf-8") as CfgFile:
        for Key, Value in DefaultConfig.items():
            CfgFile.write(f"{Key} = {repr(Value)}\n")

Vars = Variables(str(ConfigPath), ARGUMENTS)

Vars.AddVariables(
    EnumVariable(
        "BuildConfig",
        help="Build configuration",
        default="debug",
        allowed_values=("debug", "release"),
    ),
    EnumVariable(
        "BuildArch",
        help="Target architecture",
        default="i686",
        allowed_values=tuple(GetSupportedArchitectures()),
    ),
    EnumVariable(
        "BuildType",
        help="What to build",
        default="full",
        allowed_values=("full", "kernel", "usr", "image", "bootloader"),
    ),
    EnumVariable(
        "BootType",
        help="Boot type",
        default="bios",
        allowed_values=tuple(GetSupportedBootTypes()),
    ),
    EnumVariable(
        "KernelType",
        help="Kernel type/profile (e.g., generic)",
        default="generic",
        allowed_values=tuple(GetSupportedKernelTypes()),
    ),
    EnumVariable(
        "BootSystem",
        help="Bootloader system",
        default="grub",
        allowed_values=("grub", "system"),
    ),
)

Vars.Add(
    "ProjVersion", help="Kernel version string in MAJOR.MINOR form", default="0.29"
)

def CreateHostEnvironment():
    Env = Environment(
        variables=Vars,
        ENV=os.environ,
        CFLAGS=["-std=c99"],
        STRIP="strip",
    )

    Version = str(Env["ProjVersion"])
    if Env["BuildConfig"] == "debug":
        Git = GetGitHash()
        Env["ProjVersion"] = Git if Git else Version
    else:
        Env["ProjVersion"] = Version

    if Env["BuildConfig"] == "debug":
        Env.Append(CCFLAGS=["-O0", "-DDEBUG", "-g"])
    else:
        Env.Append(CCFLAGS=["-O3", "-DRELEASE", "-s"])

    ArchitectureConfig = GetArchConfig(Env["BuildArch"])
    Env.Append(
        CPPDEFINES={
            ArchitectureConfig['Define']: None,
            "OS_VERSION": f'\\"{Env["ProjVersion"]}\\"',
        }
    )

    return Env


def CreateTargetEnvironment(HostEnv):
    Architecture = HostEnv["BuildArch"]
    ArchitectureConfig = GetArchConfig(Architecture)

    Tools, ToolPaths, Prefix = ResolveTools(Architecture)

    Desc = Prefix if Prefix else "unprefixed host tools"
    print(f"Using build tool prefix for {Architecture}: {Desc}")
    print("Resolved build tools:")
    for Key in (
            "CC", 
            "AR", 
            "AS", 
            "LD", 
            "RANLIB", 
            "STRIP"
        ):
        print(f"  {Key} {Tools[Key]} -> {ToolPaths[Key]}")

    Env = HostEnv.Clone(
        **Tools,
        ArchitectureConfig=ArchitectureConfig,
        TargetTriple=ArchitectureConfig["TargetTriple"],
    )

    Env.Replace(
        ASCOMSTR=    "   AS      $SOURCE",
        ASPPCOMSTR=  "   AS      $SOURCE",
        CCCOMSTR=    "   CC      $SOURCE",
        SHCCCOMSTR=  "   CC      $SOURCE",
        LINKCOMSTR=  "   LD      $TARGET",
        SHLINKCOMSTR="   LD      $TARGET",
        ARCOMSTR=    "   AR      $TARGET",
        RANLIBCOMSTR="   RANLIB  $TARGET",
    )

    return Env


HostEnvironment = CreateHostEnvironment()
TargetEnvironment = CreateTargetEnvironment(HostEnvironment)

Help(Vars.GenerateHelpText(HostEnvironment))

Export("HostEnvironment")
Export("TargetEnvironment")

VariantDir = (
    f"build/{TargetEnvironment['BuildArch']}_{TargetEnvironment['BuildConfig']}"
)
BuildType = TargetEnvironment["BuildType"]
StageDir = os.path.abspath(os.path.join(VariantDir, "img"))

TargetEnvironment["ImageStagingDirectory"] = StageDir
TargetEnvironment["BootloaderComponents"] = {}
TargetEnvironment["KernelComponents"] = {}

if BuildType in ("full", "usr", "image"):
    SConscript("usr/SConscript", variant_dir=f"{VariantDir}/usr", duplicate=0)

if BuildType in ("full", "kernel", "image"):
    SConscript("kernel/SConscript", variant_dir=f"{VariantDir}/kernel", duplicate=0)

if BuildType in ("full", "bootloader", "image"):
    SConscript("boot/SConscript", variant_dir=f"{VariantDir}/boot", duplicate=0)

if BuildType in ("full", "image"):
    SConscript("image/SConscript", variant_dir=VariantDir, duplicate=0)
