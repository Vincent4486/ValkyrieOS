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
SOURCE_EXTENSIONS = {".c", ".h"}
PYTHON_EXTENSIONS = {".py"}
SCONS_FILENAMES = {"SConstruct", "SConscript"}

RUFF_CONFIG_PATH = Path(__file__).resolve().parent.parent / "format" / "ruff.toml"
CLANG_CONFIG_PATH = (
    Path(__file__).resolve().parent.parent / "format" / "clang-format.yaml"
)

# Directories to skip
SKIP_DIRS = {"build", "toolchain", ".git", "__pycache__", "node_modules"}


def ClassifyFile(filepath: str):
    name = Path(filepath).name
    ext = Path(filepath).suffix.lower()
    if ext in SOURCE_EXTENSIONS:
        return "clang"
    if ext in PYTHON_EXTENSIONS or name in SCONS_FILENAMES:
        return "ruff"
    return None


def FindSourceFiles(root_dir: str) -> dict:
    files = {"clang": [], "ruff": []}

    for dir_path, dir_names, file_names in os.walk(root_dir):
        # Remove directories to skip
        dir_names[:] = [d for d in dir_names if d not in SKIP_DIRS]

        for file_name in file_names:
            file_path = Path(dir_path) / file_name
            kind = ClassifyFile(file_path)
            if kind:
                files[kind].append(file_path)

    return files


def FormatCFiles(
    files: list,
    formatter: str = "clang-format",
    config_path: Path = None,
    check_only: bool = False,
    verbose: bool = False,
) -> int:
    if not files:
        print("No C/header files found.")
        return 0

    # Build command
    cmd = [formatter]
    if config_path and config_path.is_file():
        cmd.extend(["--style", f"file:{config_path}"])
    if check_only:
        cmd.extend(["--dry-run", "--Werror"])
    else:
        cmd.append("-i")

    errors = 0

    for file_path in files:
        if verbose:
            action = "Checking" if check_only else "Formatting"
            print(f"{action}: {file_path}")

        result = subprocess.run(cmd + [file_path], capture_output=True)
        if result.returncode != 0:
            errors += 1
            if check_only:
                print(f"Needs formatting: {file_path}")
            else:
                print(f"Error formatting: {file_path}")
                if result.stderr:
                    print(result.stderr.decode())

    if check_only:
        if errors > 0:
            print(f"\n{errors} file(s) need formatting.")
            return 1
        else:
            print("All C/header files properly formatted.")
            return 0
    else:
        if errors > 0:
            print(f"\n{errors} file(s) had errors.")
            return 1
        else:
            print(f"Formatted {len(files)} C/header file(s).")
            return 0


def FormatPythonFiles(
    files: list,
    formatter: str = "ruff",
    config_path: Path = None,
    check_only: bool = False,
    verbose: bool = False,
) -> int:
    if not files:
        print("No Python/SCons files found.")
        return 0

    cmd = [formatter, "format"]
    if config_path:
        cmd.extend(["--config", str(config_path)])
    if check_only:
        cmd.append("--check")

    errors = 0

    for file_path in files:
        if verbose:
            action = "Checking" if check_only else "Formatting"
            print(f"{action}: {file_path}")

        result = subprocess.run(cmd + [file_path], capture_output=True)
        if result.returncode != 0:
            errors += 1
            if check_only:
                print(f"Needs formatting: {file_path}")
            else:
                print(f"Error formatting: {file_path}")
            if result.stderr:
                print(result.stderr.decode())

    if check_only:
        if errors > 0:
            print(f"\n{errors} file(s) need formatting.")
            return 1
        else:
            print("All Python/SCons files properly formatted.")
            return 0
    else:
        if errors > 0:
            print(f"\n{errors} file(s) had errors.")
            return 1
        else:
            print(f"Formatted {len(files)} Python/SCons file(s).")
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

    args = parser.parse_args()

    # Collect all files to format
    clang_files = []
    ruff_files = []
    for path in map(Path, args.paths):
        if path.is_file():
            kind = ClassifyFile(path)
            if kind == "clang":
                clang_files.append(path)
            elif kind == "ruff":
                ruff_files.append(path)
            else:
                print(f"Warning: Unsupported file type: {path}", file=sys.stderr)
        elif path.is_dir():
            found = FindSourceFiles(path)
            clang_files.extend(found["clang"])
            ruff_files.extend(found["ruff"])
        else:
            print(f"Warning: Path not found: {path}", file=sys.stderr)

    if not clang_files and not ruff_files:
        print("No files to format.")
        sys.exit(0)

    c_result = FormatCFiles(
        files=clang_files,
        formatter=args.formatter,
        config_path=CLANG_CONFIG_PATH,
        check_only=args.check,
        verbose=args.verbose,
    )
    ruff_result = FormatPythonFiles(
        files=ruff_files,
        config_path=RUFF_CONFIG_PATH,
        check_only=args.check,
        verbose=args.verbose,
    )

    if c_result != 0:
        print(f"Cannot format C files, failed with code {c_result}.", file=sys.stderr)
        sys.exit(result)

    if ruff_result != 0:
        print(f"Cannot format Python files, failed with code {ruff_result}.", file=sys.stderr)
        sys.exit(result)


if __name__ == "__main__":
    main()
