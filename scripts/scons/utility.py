# SPDX-License-Identifier: BSD-3-Clause

from decimal import Decimal
import re
import os
import shutil
import subprocess

from SCons.Node.FS import Dir, File, Entry
from SCons.Environment import Environment


def ParseSize(Size: str) -> int:
    SizeMatch = re.match(r"([0-9\.]+)([kmg]?)", Size, re.IGNORECASE)
    if SizeMatch is None:
        raise ValueError(f"Error: Invalid size {Size}")

    Result = Decimal(SizeMatch.group(1))
    Multiplier = SizeMatch.group(2).lower()

    Multipliers = {"k": 1024, "m": 1024**2, "g": 1024**3}
    if Multiplier in Multipliers:
        Result *= Multipliers[Multiplier]

    return int(Result)


def GlobRecursive(Env: Environment, Pattern: str, Node: str = ".") -> list:
    Source = str(Env.Dir(Node).srcnode())
    WorkingDirectory = str(Env.Dir(".").srcnode())

    DirectoryList = [Source]
    for Root, Directories, _ in os.walk(Source):
        for Directory in Directories:
            DirectoryList.append(os.path.join(Root, Directory))

    GlobResults = []
    for Directory in DirectoryList:
        Matched = Env.Glob(
            os.path.join(os.path.relpath(Directory, WorkingDirectory), Pattern)
        )
        try:
            GlobResults.extend(list(Matched))
        except TypeError:
            GlobResults.append(Matched)

    return GlobResults


def GlobSources(SourcePath: str, Extensions: tuple = (".c", ".cpp", ".S")) -> list:
    Sources = []
    for Root, _Directories, Files in os.walk(SourcePath):
        for FileName in Files:
            if FileName.endswith(Extensions):
                FullPath = os.path.join(Root, FileName)
                RelativePath = os.path.relpath(FullPath, SourcePath)
                Sources.append(RelativePath)
    return Sources


def FindIndex(TheList: list, Predicate) -> int:
    for Index, Item in enumerate(TheList):
        if Predicate(Item):
            return Index
    return None


def IsFileName(Object, Name: str) -> bool:
    if isinstance(Object, str):
        return Name in Object
    elif isinstance(Object, (File, Dir, Entry)):
        return Object.name == Name
    return False


def RemoveSuffix(String: str, Suffix: str) -> str:
    if String.endswith(Suffix):
        return String[: -len(Suffix)]
    return String


def CreateBuildEnv(
    BaseEnvironment: Environment, SourcePath: str, **KeywordArgs
) -> Environment:
    EnvironmentObject = BaseEnvironment.Clone()

    EnvironmentObject.Append(
        CPATH=[SourcePath],
        CPPPATH=[SourcePath],
    )

    if KeywordArgs:
        EnvironmentObject.Append(**KeywordArgs)

    return EnvironmentObject


def GetGitHash() -> str:
    try:
        Result = subprocess.run(
            ["git", "rev-parse", "--short=7", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        )
        return Result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def ResolveTools(Arch: str):
    Prefixes = [
        f"{Arch}-linux-musl-", # Standard toolchain 
        f"{Arch}-elf-",        # Without libc
    ]

    Selected = ""
    for Prefix in Prefixes:
        Gcc = f"{Prefix}gcc" if Prefix else "gcc" # Host is target, no cross compilation needed
        if shutil.which(Gcc):
            Selected = Prefix
            break

    Bases = {
        "AS":     "as",
        "AR":     "ar",
        "CC":     "gcc",
        "LD":     "gcc",
        "RANLIB": "ranlib",
        "STRIP":  "strip",
    }

    Tools = {}
    Paths = {}

    for Key, Base in Bases.items():
        Preferred = f"{Selected}{Base}" if Selected else Base
        PreferredPath = shutil.which(Preferred)
        if PreferredPath:
            Tools[Key] = Preferred
            Paths[Key] = PreferredPath
            continue

        FallbackPath = shutil.which(Base)
        if FallbackPath:
            Tools[Key] = Base
            Paths[Key] = FallbackPath
        else:
            Tools[Key] = Preferred
            Paths[Key] = "<not found>"

    return Tools, Paths, Selected