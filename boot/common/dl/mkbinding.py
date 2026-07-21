#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Generate the bootloader binding header.

Scans boot/common/ for public C functions matching the naming convention
PREFIX_Name (e.g. FAT_Open, DISK_Read) and produces dlbind_gen.h with:
  - Function pointer declarations (initialised to NULL)
  - An inline dl_resolve_all() that calls dlsym() for each symbol

Usage: mkbinding.py <dlbind_gen.h>
"""

import os
import re
import sys

# Root of the repo  (mkbinding.py is at boot/common/dl/mkbinding.py)
ScriptDir = os.path.dirname(os.path.abspath(__file__))
RepoRoot = os.path.normpath(os.path.join(ScriptDir, "..", "..", ".."))
CommonDir = os.path.join(RepoRoot, "boot", "common")

# Regex matching a public function definition:
#   return_type  FUNC_NAME (  params  )  {
#
# FUNC_NAME must match the convention:  PREFIX_Name  where PREFIX is
# full caps or PascalCase (e.g. FAT_Open, DISK_Read, MBR_Probe, LOGO_GetValecium).
FuncRe = re.compile(
    r"^"
    r"(?:(?:const|unsigned|signed|long|short|struct|volatile|enum|extern)\s+)*"
    r"(?:int|bool|void|uint(?:8|16|32|64)_t|char|size_t|off_t|ssize_t|"
    r"int(?:8|16|32|64)_t|uintptr_t|intptr_t)"
    r"(?:\s+[*]+\s*|\s+)"
    r"(?!if|while|for|switch|return|sizeof|defined)"
    r"([A-Z0-9_]+_[A-Z][a-zA-Z0-9_]*)"
    r"\s*\(",
    re.MULTILINE,
)

# Return-type keywords we recognise (used for backwards-scanning).
RetTypeKeywords = {
    "int",
    "bool",
    "void",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "char",
    "size_t",
    "off_t",
    "ssize_t",
    "uintptr_t",
    "intptr_t",
    "const",
    "unsigned",
    "signed",
    "long",
    "short",
    "struct",
    "volatile",
    "enum",
}


def StripComments(Text: str) -> str:
    """Remove C // and /* */ comments from *Text*."""
    Text = re.sub(r"//[^\n]*", "", Text)
    Text = re.sub(r"/\*.*?\*/", "", Text, flags=re.DOTALL)
    return Text


def FindMatchingParen(Text: str, Start: int) -> int:
    """Return the index of the ')' matching the '(' at *Start*."""
    Depth = 1
    I = Start + 1
    while I < len(Text) and Depth:
        Ch = Text[I]
        if Ch == "(":
            Depth += 1
        elif Ch == ")":
            Depth -= 1
        elif Ch in ('"', "'"):
            # skip string/char literals
            Delim = Ch
            I += 1
            while I < len(Text) and Text[I] != Delim:
                if Text[I] == "\\":
                    I += 1
                I += 1
        I += 1
    return I - 1  # index of ')'


def ExtractFullSignature(Text: str, MatchEnd: int, CloseParen: int) -> str:
    """Grab the complete signature line(s) from return-type to ')'.

    We walk backward from MatchEnd to pick up qualifiers, then forward
    to the closing paren, and normalise whitespace.
    """
    # Walk backwards over whitespace, '*' and qualifiers to find the
    # beginning of the return type.  Stop at a newline that follows
    # a complete previous statement (semicolon or '{' or blank line).
    Start = MatchEnd
    while Start > 0:
        Ch = Text[Start - 1]
        if Ch in " \t":
            Start -= 1
            continue
        if Ch == "*":
            Start -= 1
            continue
        # check if preceding word is a type keyword
        # find the beginning of the current/previous word
        WEnd = Start
        while WEnd > 0 and Text[WEnd - 1].isalnum() or Text[WEnd - 1] == "_":
            WEnd -= 1
        Word = Text[WEnd:Start].strip()
        if Word in RetTypeKeywords:
            Start = WEnd
            continue
        break

    Sig = Text[Start : CloseParen + 1]
    # Normalise whitespace: collapse runs of spaces, remove space before ',' and ')'
    Sig = re.sub(r"\s+", " ", Sig)
    Sig = Sig.replace(" ,", ",").replace("( ", "(").replace(" )", ")")
    # Ensure pointer stars attach to the type, not the name
    Sig = re.sub(r"(\w)\s+\*", r"\1 *", Sig)
    # Strip leading 'extern' if present
    Sig = re.sub(r"^extern\s+", "", Sig)
    return Sig


def IsStatic(Text: str, MatchStart: int) -> bool:
    """Return True if the function is preceded by 'static'."""
    Before = Text[max(0, MatchStart - 64) : MatchStart]
    Words = Before.split()
    return "static" in Words


# Core functions that live in the stage2 binary and must NOT be resolved
# from the stage3 .so (which does not export them).
_CoreOnlySymbols = {"DL_LoadLibrary", "DL_LoadSymbol"}


def FindPublicFunctions() -> list[tuple[str, str]]:
    """Walk boot/common/ and return list of (name, full_signature)."""
    Functions: list[tuple[str, str]] = []
    SeenNames: set[str] = set()

    for Root, _Dirs, Files in os.walk(CommonDir):
        for Fn in sorted(Files):
            if not Fn.endswith(".c"):
                continue
            FilePath = os.path.join(Root, Fn)
            with open(FilePath) as fh:
                Raw = fh.read()

            Text = StripComments(Raw)
            # Also remove string literals to avoid false positives
            Text = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', Text)

            for M in FuncRe.finditer(Text):
                Name = M.group(1)
                MatchEnd = M.end()
                CloseParen = FindMatchingParen(Text, MatchEnd - 1)

                # Only include actual function definitions (followed by '{'),
                # not extern declarations (which end with ';').
                Rest = Text[CloseParen + 1 :].lstrip()
                if not Rest.startswith("{"):
                    continue

                # Skip static functions
                if IsStatic(Text, M.start()):
                    continue

                # Skip core-only symbols (defined in core, not in stage3 .so)
                if Name in _CoreOnlySymbols:
                    continue

                # Skip duplicates (e.g. same name across multiple files)
                if Name in SeenNames:
                    continue
                SeenNames.add(Name)

                Sig = ExtractFullSignature(Text, M.start(), CloseParen)
                Functions.append((Name, Sig))

    return Functions


def FunctionPointerDecl(Sig: str, Name: str) -> str:
    """Turn 'int FAT_Open(const char *path)' into 'int (*FAT_Open)(const char *path)'."""
    Idx = Sig.index(Name)
    RetType = Sig[:Idx].strip()
    Params = Sig[Idx + len(Name) :].strip()  # e.g. "(const char *path)"
    return f"{RetType} (*{Name}){Params}"


def FunctionPointerType(Sig: str, Name: str) -> str:
    """Cast-only form: 'int FAT_Open(const char *path)' -> 'int (*)(const char *path)'."""
    Idx = Sig.index(Name)
    RetType = Sig[:Idx].strip()
    Params = Sig[Idx + len(Name) :].strip()
    return f"{RetType} (*){Params}"


def GenerateHeader(Functions: list[tuple[str, str]]) -> str:
    """Produce the complete dlbind_gen.h content."""
    Lines = [
        "// !!! THIS FILE IS AUTOGENERATED by mkbinding.py !!!",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "#include <dl/loader.h>",
        "// MainBootOperations \u2013 one-shot resolved struct of bootloader function pointers.",
        "typedef struct MainBootOperations",
        "{",
    ]

    # --- struct members ---
    for Name, Sig in Functions:
        Fp = FunctionPointerDecl(Sig, Name)
        Lines.append(f"    {Fp};")

    Lines += [
        "} MainBootOperations;",
        "",
        "#ifdef DL_RESOLVE",
        "",
        "// Public struct - populated by dl_resolve_all().",
        "MainBootOperations g_MainBootOperations = {0};",
        "",
        "// Resolver - call once during init. Returns 0 on success, -1 on failure.",
        "static inline int dl_resolve_all(void *handle)",
        "{",
        "    g_MainBootOperations = (MainBootOperations){0};",
    ]

    # --- dlsym calls ---
    for Name, _Sig in Functions:
        FpType = FunctionPointerType(_Sig, Name)
        Lines.append(
            f'    g_MainBootOperations.{Name} = ({FpType})DL_LoadSymbol(handle, "{Name}");'
        )
        Lines.append(f"    if (!g_MainBootOperations.{Name}) return -1;")

    Lines += [
        "    return 0;",
        "}",
        "",
        "#else",
        "",
        "// Extern declaration \u2013 defined in the DL_RESOLVE compilation unit (main.c in core).",
        "extern MainBootOperations g_MainBootOperations;",
        "#endif /* DL_RESOLVE */",
        "",
    ]

    return "\n".join(Lines) + "\n"


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <dlbind_gen.h>", file=sys.stderr)
        sys.exit(1)

    OutH = sys.argv[1]

    Functions = FindPublicFunctions()
    Functions.sort(key=lambda t: t[0])

    Header = GenerateHeader(Functions)

    with open(OutH, "w") as f:
        f.write(Header)


if __name__ == "__main__":
    main()
