#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Cross-compiler toolchain builder for Valecium OS.

Builds binutils, musl, and GCC for the specified target architecture.
Runtime libraries are installed into a target sysroot.
"""

import argparse
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

sys.path.insert(
    0, str(Path(__file__).absolute().parent.parent.parent)
)

from scripts.scons.arch import GetArchConfig, GetSupportedArchitectures


VERSIONS = {
    "binutils": "2.45",
    "gcc": "15.2.0",
    "musl": "1.2.6",
    "linux": "6.12.7",
}

URLS = {
    "binutils": "https://ftp.gnu.org/gnu/binutils/binutils-{version}.tar.xz",
    "gcc": "https://ftp.gnu.org/gnu/gcc/gcc-{version}/gcc-{version}.tar.xz",
    "musl": "https://musl.libc.org/releases/musl-{version}.tar.gz",
    "linux": "https://cdn.kernel.org/pub/linux/kernel/v{major}.x/linux-{version}.tar.xz",
}


def GetCpuCount() -> int:
    return multiprocessing.cpu_count()


def DetectOs() -> str:
    return platform.system()


def RunCommand(
    cmd: list, env: dict = None, cwd: str = None, check: bool = True
) -> subprocess.CompletedProcess:
    print(f"  $ {' '.join(cmd)}")
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)

    return subprocess.run(cmd, env=merged_env, cwd=cwd, check=check)


def DownloadFile(url: str, dest: str):
    print(f"Downloading: {url}")

    def ProgressHook(block_num, block_size, total_size):
        downloaded = block_num * block_size
        if total_size > 0:
            percent = min(100, downloaded * 100 // total_size)
            bar = "=" * (percent // 2) + " " * (50 - percent // 2)
            print(f"\r  [{bar}] {percent}%", end="", flush=True)

    urllib.request.urlretrieve(url, dest, ProgressHook)
    print()

def ExtractArchive(archive: str, dest_dir: str):
    print(f"Extracting: {archive}")
    with tarfile.open(archive) as tar:
        tar.extractall(dest_dir)


class ToolchainBuilder:
    def __init__(self, prefix: str, target: str, jobs: int = None):
        self.prefix = Path(prefix).resolve()
        self.target = target
        self.jobs = jobs or GetCpuCount()

        self.bin_dir = self.prefix / "bin"
        self.sysroot = self.prefix / self.target

        self.srcpath = self.prefix / "src"
        self.build_dir = self.prefix / "build"

        self.build_env = {
            "PATH": f"{self.bin_dir}:{os.environ.get('PATH', '')}",
        }

    def _resolve_url(self, pkg: str, version: str) -> str:
        major = version.split(".")[0]
        return URLS[pkg].format(version=version, major=major)

    def _get_configure_opts(self, pkg: str) -> list:
        return []

    def _target_to_linux_arch(self) -> str:
        mapping = {
            "i686": "x86",
            "x86_64": "x86",
            "aarch64": "arm64",
        }
        for prefix, arch in mapping.items():
            if self.target.startswith(prefix):
                return arch
        raise ValueError(f"Cannot determine Linux ARCH for target: {self.target}")

    
    def SetupDirectories(self):
        self.prefix.mkdir(parents=True, exist_ok=True)
        self.srcpath.mkdir(exist_ok=True)
        self.build_dir.mkdir(exist_ok=True)
        self.sysroot.mkdir(parents=True, exist_ok=True)
        (self.sysroot / "usr").mkdir(exist_ok=True)

    def DownloadSources(self):
        for pkg, version in VERSIONS.items():
            url = self._resolve_url(pkg, version)
            file_name = url.split("/")[-1]
            dest = self.srcpath / file_name

            if dest.exists():
                print(f"Already downloaded: {file_name}")
                continue

            DownloadFile(url, str(dest))

    def ExtractSources(self):
        for pkg, version in VERSIONS.items():
            url = self._resolve_url(pkg, version)
            file_name = url.split("/")[-1]
            archive = self.srcpath / file_name

            # Determine extracted directory name
            src_name = f"{pkg}-{version}"

            src_path = self.srcpath / src_name
            if src_path.exists():
                print(f"Already extracted: {src_name}")
                continue

            try:
                ExtractArchive(str(archive), str(self.srcpath))
            except (EOFError, Exception) as e:
                # If extraction fails, remove the corrupted archive
                print(f"Error extracting {file_name}: {e}")
                print(f"Removing corrupted archive: {archive}")
                archive.unlink()
                raise

    def BuildLinuxHeaders(self):
        print("\n" + "=" * 60)
        print("Building Linux kernel headers")
        print("=" * 60)

        version = VERSIONS["linux"]
        url = self._resolve_url("linux", version)
        archive_name = url.split("/")[-1]
        archive = self.srcpath / archive_name
        src_path = self.srcpath / f"linux-{version}"

        if (self.sysroot / "usr" / "include" / "linux" / "kernel.h").exists():
            print("Linux headers already installed, skipping...")
            return

        # Download
        if not archive.exists():
            DownloadFile(url, str(archive))

        # Extract
        if not src_path.exists():
            ExtractArchive(str(archive), str(self.srcpath))

        # Install headers
        linux_arch = self._target_to_linux_arch()
        print(f"  ARCH={linux_arch} INSTALL_HDR_PATH={self.sysroot / 'usr'}")
        RunCommand(
            [
                "make",
                f"ARCH={linux_arch}",
                f"INSTALL_HDR_PATH={self.sysroot / 'usr'}",
                "headers_install",
            ],
            cwd=str(src_path),
        )

    def BuildBinutils(self):
        print("\n" + "=" * 60)
        print("Building binutils")
        print("=" * 60)

        version = VERSIONS["binutils"]
        src_path = self.srcpath / f"binutils-{version}"
        build_path = self.build_dir / f"binutils-{self.target}"

        if (self.bin_dir / f"{self.target}-as").exists():
            print("binutils already installed, skipping...")
            return

        build_path.mkdir(exist_ok=True)

        clean_env = {
            "CFLAGS": "",
            "ASMFLAGS": "",
            "CC": "",
            "CXX": "",
            "LD": "",
            "ASM": "",
            "LINKFLAGS": "",
            "LIBS": "",
        }

        configure_opts = [
            f"--prefix={self.prefix}",
            f"--target={self.target}",
            "--disable-nls",
            "--disable-werror",
        ] + self._get_configure_opts("binutils")

        RunCommand(
            [str(src_path / "configure")] + configure_opts,
            env={**self.build_env, **clean_env},
            cwd=str(build_path),
        )

        RunCommand(["make", f"-j{self.jobs}"], cwd=str(build_path))
        RunCommand(["make", "install"], cwd=str(build_path))

    def BuildGccStage1(self):
        print("\n" + "=" * 60)
        print("Building GCC Stage 1")
        print("=" * 60)

        version = VERSIONS["gcc"]
        src_path = self.srcpath / f"gcc-{version}"
        build_path = self.build_dir / f"gcc-stage1-{self.target}"

        if (self.bin_dir / f"{self.target}-gcc").exists():
            print("GCC stage 1 already installed, skipping...")
            return

        build_path.mkdir(exist_ok=True)

        configure_opts = [
            f"--prefix={self.prefix}",
            f"--target={self.target}",
            "--disable-nls",
            "--enable-languages=c",
            "--without-headers",
            "--disable-threads",
            "--disable-isl",
            "--disable-shared",
            "--with-newlib",
            f"--with-sysroot={self.sysroot}",
            f"--with-build-sysroot={self.sysroot}",
            "--with-native-system-header-dir=/usr/include",
        ] + self._get_configure_opts("gcc")

        RunCommand(
            [str(src_path / "configure")] + configure_opts,
            env=self.build_env,
            cwd=str(build_path),
        )

        RunCommand(
            ["make", f"-j{self.jobs}", "all-gcc", "all-target-libgcc"],
            cwd=str(build_path),
        )
        RunCommand(
            ["make", "install-gcc", "install-target-libgcc"],
            cwd=str(build_path),
        )

    def BuildMusl(self):
        print("\n" + "=" * 60)
        print("Building musl")
        print("=" * 60)

        version = VERSIONS["musl"]
        src_path = self.srcpath / f"musl-{version}"
        build_path = self.build_dir / f"musl-{self.target}"
        libc_archive = self.sysroot / "usr" / "lib" / "libc.so"

        if libc_archive.exists():
            print("musl already installed in sysroot, skipping...")
            return

        build_path.mkdir(exist_ok=True)

        cross_env = {
            **self.build_env,
            "CC": f"{self.target}-gcc",
            "AR": f"{self.target}-ar",
            "RANLIB": f"{self.target}-ranlib",
        }

        configure_opts = [
            "--prefix=/usr",
            f"--host={self.target}",
            "--enable-static",
            "--enable-shared",
        ]

        RunCommand(
            [str(src_path / "configure")] + configure_opts,
            env=cross_env,
            cwd=str(build_path),
        )

        RunCommand(["make", f"-j{self.jobs}"], env=cross_env, cwd=str(build_path))
        RunCommand(
            ["make", "install", f"DESTDIR={self.sysroot}"],
            env=cross_env,
            cwd=str(build_path),
        )

    def BuildGccStage2(self):
        """Build GCC stage 2 against the populated sysroot."""
        print("\n" + "=" * 60)
        print("Building GCC Stage 2")
        print("=" * 60)

        version = VERSIONS["gcc"]
        src_path = self.srcpath / f"gcc-{version}"
        build_path = self.build_dir / f"gcc-stage2-{self.target}"

        build_path.mkdir(exist_ok=True)

        configure_opts = [
            f"--prefix={self.prefix}",
            f"--target={self.target}",
            "--disable-nls",
            "--enable-languages=c",
            "--disable-libsanitizer",
            "--enable-shared",
            f"--with-sysroot={self.sysroot}",
            f"--with-build-sysroot={self.sysroot}",
            "--with-native-system-header-dir=/usr/include",
        ] + self._get_configure_opts("gcc")

        RunCommand(
            [str(src_path / "configure")] + configure_opts,
            env=self.build_env,
            cwd=str(build_path),
        )

        RunCommand(["make", f"-j{self.jobs}"], cwd=str(build_path))
        RunCommand(["make", "install"], cwd=str(build_path))

    def GetRuntimeSysroot(self) -> Path:
        return self.sysroot

    def BuildAll(self):
        print(f"Building toolchain for {self.target}")
        print(f"  Prefix: {self.prefix}")
        print(f"  Jobs: {self.jobs}")
        print()

        if self.IsInstalled():
            print("Toolchain sysroot already installed, skipping bootstrap...")
            print(f"  Sysroot: {self.sysroot}")
            return

        self.SetupDirectories()
        self.DownloadSources()
        self.ExtractSources()

        self.BuildBinutils()
        self.BuildGccStage1()
        self.BuildLinuxHeaders()
        self.BuildMusl()
        self.BuildGccStage2()

        print("\n" + "=" * 60)
        print("Toolchain build complete!")
        print("=" * 60)
        print(f'\nAdd to PATH: export PATH="{self.bin_dir}:$PATH"')
        print(f"Runtime sysroot: {self.GetRuntimeSysroot()}")
        print("Copy this sysroot content into image root during OS image assembly.")

        # Clean up build and source directories
        print("\nCleaning up build and source directories...")
        self.CleanAll()

    def Clean(self):
        print("Cleaning build directories...")
        if self.build_dir.exists():
            shutil.rmtree(self.build_dir)
            print(f"Removed: {self.build_dir}")

    def CleanAll(self):
        print("Cleaning everything...")
        for path in [self.build_dir, self.srcpath]:
            if path.exists():
                shutil.rmtree(path)
                print(f"Removed: {path}")

    def IsInstalled(self) -> bool:
        required_tools = [
            self.bin_dir / f"{self.target}-as",
            self.bin_dir / f"{self.target}-gcc",
            self.sysroot / "usr" / "lib" / "libc.so",
            self.sysroot / "usr" / "include" / "linux" / "kernel.h",
        ]
        return all(path.exists() for path in required_tools)


def main():
    parser = argparse.ArgumentParser(
        description="Build cross-compilation toolchain for Valecium OS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("prefix", help="Toolchain installation prefix")
    parser.add_argument(
        "-a",
        "--arch",
        choices=GetSupportedArchitectures(),
        help="Target architecture (uses predefined target triple)",
    )
    parser.add_argument(
        "-t", "--target", help="Custom target triple (overrides --arch)"
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=GetCpuCount(),
        help=f"Parallel jobs (default: {GetCpuCount()})",
    )
    parser.add_argument("--clean", action="store_true", help="Remove build directories")
    parser.add_argument(
        "--clean-all",
        action="store_true",
        help="Remove all build artifacts and sources",
    )
    parser.add_argument(
        "--binutils-only", action="store_true", help="Build only binutils"
    )
    parser.add_argument(
        "--gcc-stage1-only", action="store_true", help="Build only GCC stage 1"
    )
    args = parser.parse_args()

    # Determine target triple
    if args.target:
        target = args.target
    elif args.arch:
        target = GetArchConfig(args.arch)["TargetTriple"]
    else:
        target = GetArchConfig("i686")["TargetTriple"]

    builder = ToolchainBuilder(
        prefix=args.prefix,
        target=target,
        jobs=args.jobs,
    )

    try:
        if args.clean_all:
            builder.CleanAll()
        elif args.clean:
            builder.Clean()
        elif args.binutils_only:
            builder.SetupDirectories()
            builder.DownloadSources()
            builder.ExtractSources()
            builder.BuildBinutils()
        elif args.gcc_stage1_only:
            builder.SetupDirectories()
            builder.DownloadSources()
            builder.ExtractSources()
            builder.BuildGccStage1()
        else:
            builder.BuildAll()
    except subprocess.CalledProcessError as exc:
        print(
            f"\nError: Command failed with exit code {exc.returncode}", file=sys.stderr
        )
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nBuild interrupted.")
        sys.exit(130)


if __name__ == "__main__":
    main()
