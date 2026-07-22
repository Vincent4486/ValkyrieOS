#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Code formatter for Valecium OS.

Formats C/header files using clang-format and Python/SCons files using ruff.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


# File extensions to format
SourceExtensions = {".c", ".h"}
PythonExtensions = {".py"}
SconsFilenames = {"SConstruct", "SConscript"}

RuffConfigPath = Path(__file__).resolve().parent.parent / "format" / "ruff.toml"
ClangConfigPath = (
    Path(__file__).resolve().parent.parent / "format" / "clang-format.yaml"
)

# Directories to skip
SkipDirs = {"build", "toolchain", ".git", "__pycache__", "node_modules"}


def ClassifyFile(Filepath: str):
    """Classify a file for clang-format or ruff."""
    Name = Path(Filepath).name
    Ext = Path(Filepath).suffix.lower()
    if Ext in SourceExtensions:
        return "clang"
    if Ext in PythonExtensions or Name in SconsFilenames:
        return "ruff"
    return None


def FindSourceFiles(RootDir: str) -> dict:
    """Find all supported source files in a directory tree.

    Args:
        RootDir: Root directory to search

    Returns:
        Dict with keys "clang" and "ruff"
    """
    Files = {"clang": [], "ruff": []}

    for DirPath, DirNames, FileNames in os.walk(RootDir):
        # Remove directories to skip
        DirNames[:] = [d for d in DirNames if d not in SkipDirs]

        for FileName in FileNames:
            FilePath = os.path.join(DirPath, FileName)
            Kind = ClassifyFile(FilePath)
            if Kind:
                Files[Kind].append(FilePath)

    return Files


def FormatCFiles(
    Files: list,
    Formatter: str = "clang-format",
    ConfigPath: Path = None,
    CheckOnly: bool = False,
    Verbose: bool = False,
) -> int:
    """Format C/header files using clang-format.

    Args:
        Files: List of file paths to format
        Formatter: Formatter command name
        ConfigPath: Path to a .clang-format config file
        CheckOnly: If True, only check formatting without modifying
        Verbose: Print each file being processed

    Returns:
        0 if successful, 1 if changes needed (CheckOnly) or errors
    """
    if not Files:
        print("No C/header files found.")
        return 0

    # Build command
    Cmd = [Formatter]
    if ConfigPath and ConfigPath.is_file():
        Cmd.extend(["--style", f"file:{ConfigPath}"])
    if CheckOnly:
        Cmd.extend(["--dry-run", "--Werror"])
    else:
        Cmd.append("-i")

    Errors = 0

    for FilePath in Files:
        if Verbose:
            Action = "Checking" if CheckOnly else "Formatting"
            print(f"{Action}: {FilePath}")

        Result = subprocess.run(Cmd + [FilePath], capture_output=True)
        if Result.returncode != 0:
            Errors += 1
            if CheckOnly:
                print(f"Needs formatting: {FilePath}")
            else:
                print(f"Error formatting: {FilePath}")
                if Result.stderr:
                    print(Result.stderr.decode())

    if CheckOnly:
        if Errors > 0:
            print(f"\n{Errors} file(s) need formatting.")
            return 1
        else:
            print("All C/header files properly formatted.")
            return 0
    else:
        if Errors > 0:
            print(f"\n{Errors} file(s) had errors.")
            return 1
        else:
            print(f"Formatted {len(Files)} C/header file(s).")
            return 0


def FormatPythonFiles(
    Files: list,
    Formatter: str = "ruff",
    ConfigPath: Path = None,
    CheckOnly: bool = False,
    Verbose: bool = False,
) -> int:
    """Format Python/SCons files using ruff.

    Args:
        Files: List of file paths to format
        Formatter: Formatter command name
        ConfigPath: Path to ruff.toml config
        CheckOnly: If True, only check formatting without modifying
        Verbose: Print each file being processed

    Returns:
        0 if successful, 1 if changes needed (CheckOnly) or errors
    """
    if not Files:
        print("No Python/SCons files found.")
        return 0

    Cmd = [Formatter, "format"]
    if ConfigPath:
        Cmd.extend(["--config", str(ConfigPath)])
    if CheckOnly:
        Cmd.append("--check")

    Errors = 0

    for FilePath in Files:
        if Verbose:
            Action = "Checking" if CheckOnly else "Formatting"
            print(f"{Action}: {FilePath}")

        Result = subprocess.run(Cmd + [FilePath], capture_output=True)
        if Result.returncode != 0:
            Errors += 1
            if CheckOnly:
                print(f"Needs formatting: {FilePath}")
            else:
                print(f"Error formatting: {FilePath}")
            if Result.stderr:
                print(Result.stderr.decode())

    if CheckOnly:
        if Errors > 0:
            print(f"\n{Errors} file(s) need formatting.")
            return 1
        else:
            print("All Python/SCons files properly formatted.")
            return 0
    else:
        if Errors > 0:
            print(f"\n{Errors} file(s) had errors.")
            return 1
        else:
            print(f"Formatted {len(Files)} Python/SCons file(s).")
            return 0


def main():
    parser = argparse.ArgumentParser(
        description="Format C/header and Python/SCons files in the Valecium OS project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument(
        "paths",
        nargs="*",
        default=["."],
        help="Directories or files to format (default: current directory)",
    )
    parser.add_argument(
        "-c",
        "--check",
        action="store_true",
        help="Check formatting without modifying files",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Print each file being processed"
    )
    parser.add_argument(
        "--formatter",
        default="clang-format",
        help="Formatter command (default: clang-format)",
    )

    Args = parser.parse_args()

    # Collect all files to format
    ClangFiles = []
    RuffFiles = []
    for Path in Args.paths:
        if os.path.isfile(Path):
            Kind = ClassifyFile(Path)
            if Kind == "clang":
                ClangFiles.append(Path)
            elif Kind == "ruff":
                RuffFiles.append(Path)
            else:
                print(f"Warning: Unsupported file type: {Path}", file=sys.stderr)
        elif os.path.isdir(Path):
            Found = FindSourceFiles(Path)
            ClangFiles.extend(Found["clang"])
            RuffFiles.extend(Found["ruff"])
        else:
            print(f"Warning: Path not found: {Path}", file=sys.stderr)

    if not ClangFiles and not RuffFiles:
        print("No files to format.")
        sys.exit(0)

    CResult = FormatCFiles(
        Files=ClangFiles,
        Formatter=Args.formatter,
        ConfigPath=ClangConfigPath,
        CheckOnly=Args.check,
        Verbose=Args.verbose,
    )
    RuffResult = FormatPythonFiles(
        Files=RuffFiles,
        ConfigPath=RuffConfigPath if RuffConfigPath.is_file() else None,
        CheckOnly=Args.check,
        Verbose=Args.verbose,
    )

    sys.exit(1 if (CResult != 0 or RuffResult != 0) else 0)


if __name__ == "__main__":
    main()
