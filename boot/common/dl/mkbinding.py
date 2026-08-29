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
from pathlib import Path

SCRIPT_DIR = Path(__file__).absolute().parent
REPO_ROOT = Path(os.path.normpath(SCRIPT_DIR / ".." / ".." / ".."))
COMMON_DIR = REPO_ROOT / "boot" / "common"

FUNC_RE = re.compile(
    r"^"
    r"(?:(?:const|unsigned|signed|long|short|struct|volatile|enum|extern)\s+)*"
    r"(?:(?:int|bool|void|uint(?:8|16|32|64)_t|char|size_t|off_t|ssize_t|"
    r"int(?:8|16|32|64)_t|uintptr_t|intptr_t)|[A-Za-z_][A-Za-z0-9_]*)"
    r"(?:\s+[*]+\s*|\s+)"
    r"(?!if|while|for|switch|return|sizeof|defined)"
    r"([A-Z0-9_]+_[A-Z][a-zA-Z0-9_]*)"
    r"\s*\(",
    re.MULTILINE,
)

RET_TYPE_KEYWORDS = {
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

_CORE_ONLY_SYMBOLS = {"DL_LoadLibrary", "DL_LoadSymbol"}

_BUILTIN_TYPE_TOKENS = {
    "int", "bool", "void", "char", "size_t", "off_t", "ssize_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uintptr_t", "intptr_t",
    "const", "unsigned", "signed", "long", "short",
    "struct", "volatile", "enum", "extern",
}


def StripComments(text: str) -> str:
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return text


def FindMatchingParen(text: str, start: int) -> int:
    depth = 1
    i = start + 1
    while i < len(text) and depth:
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch in ('"', "'"):
            delim = ch
            i += 1
            while i < len(text) and text[i] != delim:
                if text[i] == "\\":
                    i += 1
                i += 1
        i += 1
    return i - 1


def ExtractFullSignature(text: str, match_end: int, close_paren: int) -> str:
    start = match_end
    while start > 0:
        ch = text[start - 1]

        if ch in " \t":
            start -= 1
            continue

        if ch == "*":
            start -= 1
            continue

        w_end = start

        while w_end > 0 and text[w_end - 1].isalnum() or text[w_end - 1] == "_":
            w_end -= 1
        
        word = text[w_end:start].strip()

        if word in RET_TYPE_KEYWORDS:
            start = w_end
            continue

        break

    sig = text[start : close_paren + 1]
    sig = re.sub(r"\s+", " ", sig)
    sig = sig.replace(" ,", ",").replace("( ", "(").replace(" )", ")")
    sig = re.sub(r"(\w)\s+\*", r"\1 *", sig)
    sig = re.sub(r"^extern\s+", "", sig)
    return sig


def IsStatic(text: str, match_start: int) -> bool:
    before = text[max(0, match_start - 64) : match_start]
    words = before.split()
    return "static" in words


def FindPublicFunctions() -> list[tuple[str, str]]:
    functions: list[tuple[str, str]] = []
    seen_names: set[str] = set()

    for root, _dirs, files in os.walk(COMMON_DIR):
        for fn in sorted(files):
            if not fn.endswith(".c"):
                continue
            file_path = Path(root) / fn
            with open(file_path) as fh:
                raw = fh.read()

            text = StripComments(raw)
            text = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', text)

            for m in FUNC_RE.finditer(text):
                name = m.group(1)
                match_end = m.end()
                close_paren = FindMatchingParen(text, match_end - 1)

                rest = text[close_paren + 1 :].lstrip()
                if not rest.startswith("{"):
                    continue

                if IsStatic(text, m.start()):
                    continue

                if name in _CORE_ONLY_SYMBOLS:
                    continue

                if name in seen_names:
                    continue
                seen_names.add(name)

                sig = ExtractFullSignature(text, m.start(), close_paren)
                functions.append((name, sig))

    return functions


def CustomReturnTypes(functions: list[tuple[str, str]]) -> set[str]:
    custom: set[str] = set()
    for name, sig in functions:
        ret_type = sig[: sig.index(name)]
        for token in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", ret_type):
            if token not in _BUILTIN_TYPE_TOKENS:
                custom.add(token)
    return custom


def FindTypeHeaders(types: set[str]) -> list[str]:
    headers: list[str] = []
    for root, _dirs, files in os.walk(COMMON_DIR):
        for fn in sorted(files):
            if fn.endswith(".h"):
                headers.append(Path(root) / fn)

    includes: set[str] = set()
    missing: set[str] = set(types)
    for type_name in sorted(types):
        struct_re = re.compile(r"}\s*" + re.escape(type_name) + r"\s*;")
        typedef_re = re.compile(
            r"\btypedef\b[^;]*?\b" + re.escape(type_name) + r"\s*;"
        )
        for h in headers:
            with open(h) as fh:
                text = fh.read()
            if struct_re.search(text) or typedef_re.search(text):
                rel = h.relative_to(COMMON_DIR)
                includes.add(str(rel).replace(os.sep, "/"))
                missing.discard(type_name)
                break

    if missing:
        raise RuntimeError(
            "mkbinding: no boot/common header defines custom return type(s): "
            + ", ".join(sorted(missing))
        )
    return sorted(includes)


def FunctionPointerDecl(sig: str, name: str) -> str:
    idx = sig.index(name)
    ret_type = sig[:idx].strip()
    params = sig[idx + len(name) :].strip()
    return f"{ret_type} (*{name}){params}"


def FunctionPointerType(sig: str, name: str) -> str:
    idx = sig.index(name)
    ret_type = sig[:idx].strip()
    params = sig[idx + len(name) :].strip()
    return f"{ret_type} (*){params}"


def GenerateHeader(functions: list[tuple[str, str]],
                   type_includes: list[str]) -> str:
    lines = [
        "// !!! THIS FILE IS AUTOGENERATED by mkbinding.py !!!",
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "#include <dl/loader.h>",
    ]
    for inc in type_includes:
        lines.append(f"#include <{inc}>")
    lines += [
        "",
        "// MainBootOperations \u2013 one-shot resolved struct of bootloader function pointers.",
        "typedef struct MainBootOperations",
        "{",
    ]

    for name, sig in functions:
        fp = FunctionPointerDecl(sig, name)
        lines.append(f"    {fp};")

    lines += [
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

    for name, _sig in functions:
        fp_type = FunctionPointerType(_sig, name)
        lines.append(
            f'    g_MainBootOperations.{name} = ({fp_type})DL_LoadSymbol(handle, "{name}");'
        )
        lines.append(f"    if (!g_MainBootOperations.{name}) return -1;")

    lines += [
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

    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <dlbind_gen.h>", file=sys.stderr)
        sys.exit(1)

    out_h = sys.argv[1]

    functions = FindPublicFunctions()
    functions.sort(key=lambda t: t[0])

    type_includes = FindTypeHeaders(CustomReturnTypes(functions))

    header = GenerateHeader(functions, type_includes)

    with open(out_h, "w") as f:
        f.write(header)


if __name__ == "__main__":
    main()
