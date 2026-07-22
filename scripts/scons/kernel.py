# SPDX-License-Identifier: BSD-3-Clause

import copy
import os
import struct

from SCons.Environment import Environment

KernelProfiles = {
    "generic": {
        "SupportedArchitectures": [
            "i686",
            "x86_64",
        ],
        "CompilerFlags": ["-DKERNEL_TYPE=\"GENERIC\""],
        "AssemblerFlags": ["-DKERNEL_TYPE=\"GENERIC\""],
    },
}

def GetSupportedKernelTypes() -> list:
    return list(KernelProfiles.keys())


def GetKernelBuildConfig(
    KernelType: str, Architecture: str, Version: str, BuildConfig: str = "debug"
) -> dict:
    Config = copy.deepcopy(KernelProfiles[KernelType])
    SupportedArchitectures = Config.get("SupportedArchitectures", [])
    if SupportedArchitectures and Architecture not in SupportedArchitectures:
        raise ValueError(
            f"Unsupported architecture {Architecture} for kernel type {KernelType}. "
            f"Supported: {SupportedArchitectures}"
        )

    Config["Architecture"] = Architecture
    Config["Version"] = Version
    Config["BuildConfig"] = BuildConfig
    Config["OutputName"] = f"valeciumx-{Version}_{BuildConfig}"

    return Config


def ConfigureKernelEnvironment(
    Env: Environment,
    SourcePath: str,
    ArchitecturePath: str,
    ArchitectureConfig: dict,
    KernelConfig: dict,
    LinkerScript: str,
):
    Env.Append(
        ASFLAGS=ArchitectureConfig.get("AssemblyFlags", []),
        CCFLAGS=ArchitectureConfig.get("CompilerFlags", []),
        LINKFLAGS=ArchitectureConfig.get("LinkerFlags", []),
    )

    Env.Append(
        CCFLAGS=[
            "-ffreestanding",
            "-nostdlib",
            "-fno-stack-protector",
            "-fno-builtin",
            "-Wall",
            "-Wextra",
        ],
        LINKFLAGS=[
            "-nostdlib",
            "-Wl,-T",
            LinkerScript,
            "-Wl,-Map=" + Env.File("kernel.map").path,
            "-Wl,-z,relro,-z,now",
            "-Wl,-z,noexecstack",
            "-Wl,--as-needed",
            "-Wl,--export-dynamic",
        ],
        CPPPATH=[
            SourcePath,
            "#include",
        ],
        ASFLAGS=["-Wa,--noexecstack"],
        LIBS=["gcc"],
        CPPDEFINES={"VALECIUM_KERNEL": None},
    )

    Env.Append(CCFLAGS=KernelConfig.get("CompilerFlags", []))
    Env.Append(ASFLAGS=KernelConfig.get("AssemblerFlags", []))
