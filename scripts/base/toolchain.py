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

# Add parent directory to path for imports
sys.path.insert(
    0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

from scripts.scons.arch import GetArchConfig, GetSupportedArchitectures


# Version Configuration

Versions = {
    "binutils": "2.45",
    "gcc": "15.2.0",
    "musl": "1.2.6",
    "linux": "6.12.7",
}

Urls = {
    "binutils": "https://ftp.gnu.org/gnu/binutils/binutils-{version}.tar.xz",
    "gcc": "https://ftp.gnu.org/gnu/gcc/gcc-{version}/gcc-{version}.tar.xz",
    "musl": "https://musl.libc.org/releases/musl-{version}.tar.gz",
    "linux": "https://cdn.kernel.org/pub/linux/kernel/v{major}.x/linux-{version}.tar.xz",
}


# Helper Functions


def GetCpuCount() -> int:
    """Get number of CPUs for parallel builds."""
    return multiprocessing.cpu_count()


def DetectOs() -> str:
    """Detect host operating system."""
    return platform.system()


def RunCommand(
    Cmd: list, Env: dict = None, Cwd: str = None, Check: bool = True
) -> subprocess.CompletedProcess:
    """Run a command with logging."""
    print(f"  $ {' '.join(Cmd)}")
    MergedEnv = os.environ.copy()
    if Env:
        MergedEnv.update(Env)

    return subprocess.run(Cmd, env=MergedEnv, cwd=Cwd, check=Check)


def DownloadFile(Url: str, Dest: str):
    """Download a file with progress indicator."""
    print(f"Downloading: {Url}")

    def ProgressHook(BlockNum, BlockSize, TotalSize):
        Downloaded = BlockNum * BlockSize
        if TotalSize > 0:
            Percent = min(100, Downloaded * 100 // TotalSize)
            Bar = "=" * (Percent // 2) + " " * (50 - Percent // 2)
            print(f"\r  [{Bar}] {Percent}%", end="", flush=True)

    urllib.request.urlretrieve(Url, Dest, ProgressHook)
    print()  # Newline after progress


def ExtractArchive(Archive: str, DestDir: str):
    """Extract a tar archive."""
    print(f"Extracting: {Archive}")
    with tarfile.open(Archive) as Tar:
        Tar.extractall(DestDir)


# Build Classes


class ToolchainBuilder:
    """Builds a complete cross-compilation toolchain."""

    def __init__(self, prefix: str, target: str, jobs: int = None):
        """
        Args:
            prefix: Toolchain installation prefix (e.g., /opt/toolchain)
            target: Target triple (e.g., i686-linux-musl)
            jobs: Number of parallel jobs (-j flag)
        """
        self.prefix = Path(prefix).resolve()
        self.target = target
        self.jobs = jobs or GetCpuCount()

        # Derived paths
        self.bin_dir = self.prefix / "bin"
        self.sysroot = self.prefix / self.target

        # Source/build directories
        self.srcpath = self.prefix / "src"
        self.build_dir = self.prefix / "build"

        # Environment for builds
        self.build_env = {
            "PATH": f"{self.bin_dir}:{os.environ.get('PATH', '')}",
        }

    def SetupDirectories(self):
        """Create necessary directories."""
        self.prefix.mkdir(parents=True, exist_ok=True)
        self.srcpath.mkdir(exist_ok=True)
        self.build_dir.mkdir(exist_ok=True)
        self.sysroot.mkdir(parents=True, exist_ok=True)
        (self.sysroot / "usr").mkdir(exist_ok=True)

    def _resolve_url(self, pkg: str, version: str) -> str:
        """Resolve the download URL for a package, handling version-specific formatting."""
        major = version.split(".")[0]
        return Urls[pkg].format(version=version, major=major)

    def DownloadSources(self):
        """Download all source tarballs."""
        for Pkg, Version in Versions.items():
            Url = self._resolve_url(Pkg, Version)
            FileName = Url.split("/")[-1]
            Dest = self.srcpath / FileName

            if Dest.exists():
                print(f"Already downloaded: {FileName}")
                continue

            DownloadFile(Url, str(Dest))

    def ExtractSources(self):
        """Extract all source tarballs."""
        for Pkg, Version in Versions.items():
            Url = self._resolve_url(Pkg, Version)
            FileName = Url.split("/")[-1]
            Archive = self.srcpath / FileName

            # Determine extracted directory name
            SrcName = f"{Pkg}-{Version}"

            SrcPath = self.srcpath / SrcName
            if SrcPath.exists():
                print(f"Already extracted: {SrcName}")
                continue

            try:
                ExtractArchive(str(Archive), str(self.srcpath))
            except (EOFError, Exception) as E:
                # If extraction fails, remove the corrupted archive
                print(f"Error extracting {FileName}: {E}")
                print(f"Removing corrupted archive: {Archive}")
                Archive.unlink()
                raise

    def _get_configure_opts(self, pkg: str) -> list:
        """Get platform-specific configure options."""
        return []

    def _target_to_linux_arch(self) -> str:
        """Map target triple to Linux kernel ARCH name."""
        mapping = {
            "i686": "x86",
            "x86_64": "x86",
            "aarch64": "arm64",
        }
        for prefix, arch in mapping.items():
            if self.target.startswith(prefix):
                return arch
        raise ValueError(f"Cannot determine Linux ARCH for target: {self.target}")

    def BuildLinuxHeaders(self):
        """Download and install Linux kernel headers into sysroot."""
        print("\n" + "=" * 60)
        print("Building Linux kernel headers")
        print("=" * 60)

        Version = Versions["linux"]
        Url = self._resolve_url("linux", Version)
        ArchiveName = Url.split("/")[-1]
        Archive = self.srcpath / ArchiveName
        SrcPath = self.srcpath / f"linux-{Version}"

        if (self.sysroot / "usr" / "include" / "linux" / "kernel.h").exists():
            print("Linux headers already installed, skipping...")
            return

        # Download
        if not Archive.exists():
            DownloadFile(Url, str(Archive))

        # Extract
        if not SrcPath.exists():
            ExtractArchive(str(Archive), str(self.srcpath))

        # Install headers
        LinuxArch = self._target_to_linux_arch()
        print(f"  ARCH={LinuxArch} INSTALL_HDR_PATH={self.sysroot / 'usr'}")
        RunCommand(
            [
                "make",
                f"ARCH={LinuxArch}",
                f"INSTALL_HDR_PATH={self.sysroot / 'usr'}",
                "headers_install",
            ],
            Cwd=str(SrcPath),
        )

    def BuildBinutils(self):
        """Build and install binutils."""
        print("\n" + "=" * 60)
        print("Building binutils")
        print("=" * 60)

        Version = Versions["binutils"]
        SrcPath = self.srcpath / f"binutils-{Version}"
        BuildPath = self.build_dir / f"binutils-{self.target}"

        if (self.bin_dir / f"{self.target}-as").exists():
            print("binutils already installed, skipping...")
            return

        BuildPath.mkdir(exist_ok=True)

        # Clean environment for configure
        CleanEnv = {
            "CFLAGS": "",
            "ASMFLAGS": "",
            "CC": "",
            "CXX": "",
            "LD": "",
            "ASM": "",
            "LINKFLAGS": "",
            "LIBS": "",
        }

        ConfigureOpts = [
            f"--prefix={self.prefix}",
            f"--target={self.target}",
            "--disable-nls",
            "--disable-werror",
        ] + self._get_configure_opts("binutils")

        RunCommand(
            [str(SrcPath / "configure")] + ConfigureOpts,
            Env={**self.build_env, **CleanEnv},
            Cwd=str(BuildPath),
        )

        RunCommand(["make", f"-j{self.jobs}"], Cwd=str(BuildPath))
        RunCommand(["make", "install"], Cwd=str(BuildPath))

    def BuildGccStage1(self):
        """Build GCC stage 1 (C only, no libc)."""
        print("\n" + "=" * 60)
        print("Building GCC Stage 1")
        print("=" * 60)

        Version = Versions["gcc"]
        SrcPath = self.srcpath / f"gcc-{Version}"
        BuildPath = self.build_dir / f"gcc-stage1-{self.target}"

        if (self.bin_dir / f"{self.target}-gcc").exists():
            print("GCC stage 1 already installed, skipping...")
            return

        BuildPath.mkdir(exist_ok=True)

        ConfigureOpts = [
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
            [str(SrcPath / "configure")] + ConfigureOpts,
            Env=self.build_env,
            Cwd=str(BuildPath),
        )

        RunCommand(
            ["make", f"-j{self.jobs}", "all-gcc", "all-target-libgcc"],
            Cwd=str(BuildPath),
        )
        RunCommand(
            ["make", "install-gcc", "install-target-libgcc"],
            Cwd=str(BuildPath),
        )

    def BuildMusl(self):
        """Build and install musl into the toolchain sysroot."""
        print("\n" + "=" * 60)
        print("Building musl")
        print("=" * 60)

        Version = Versions["musl"]
        SrcPath = self.srcpath / f"musl-{Version}"
        BuildPath = self.build_dir / f"musl-{self.target}"
        LibcArchive = self.sysroot / "usr" / "lib" / "libc.so"

        if LibcArchive.exists():
            print("musl already installed in sysroot, skipping...")
            return

        BuildPath.mkdir(exist_ok=True)

        CrossEnv = {
            **self.build_env,
            "CC": f"{self.target}-gcc",
            "AR": f"{self.target}-ar",
            "RANLIB": f"{self.target}-ranlib",
        }

        ConfigureOpts = [
            "--prefix=/usr",
            # "--syslibdir=/lib",
            f"--host={self.target}",
            "--enable-static",
            "--enable-shared",
        ]

        RunCommand(
            [str(SrcPath / "configure")] + ConfigureOpts,
            Env=CrossEnv,
            Cwd=str(BuildPath),
        )

        RunCommand(["make", f"-j{self.jobs}"], Env=CrossEnv, Cwd=str(BuildPath))
        RunCommand(
            ["make", "install", f"DESTDIR={self.sysroot}"],
            Env=CrossEnv,
            Cwd=str(BuildPath),
        )

    def BuildGccStage2(self):
        """Build GCC stage 2 against the populated sysroot."""
        print("\n" + "=" * 60)
        print("Building GCC Stage 2")
        print("=" * 60)

        Version = Versions["gcc"]
        SrcPath = self.srcpath / f"gcc-{Version}"
        BuildPath = self.build_dir / f"gcc-stage2-{self.target}"

        BuildPath.mkdir(exist_ok=True)

        ConfigureOpts = [
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
            [str(SrcPath / "configure")] + ConfigureOpts,
            Env=self.build_env,
            Cwd=str(BuildPath),
        )

        RunCommand(["make", f"-j{self.jobs}"], Cwd=str(BuildPath))
        RunCommand(["make", "install"], Cwd=str(BuildPath))

    def GetRuntimeSysroot(self) -> Path:
        """Return the sysroot whose contents should be copied into image root."""
        return self.sysroot

    def BuildAll(self):
        """Build the complete toolchain."""
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
        """Remove build directories (keep sources)."""
        print("Cleaning build directories...")
        if self.build_dir.exists():
            shutil.rmtree(self.build_dir)
            print(f"Removed: {self.build_dir}")

    def CleanAll(self):
        """Remove all build artifacts and sources."""
        print("Cleaning everything...")
        for Path in [self.build_dir, self.srcpath]:
            if Path.exists():
                shutil.rmtree(Path)
                print(f"Removed: {Path}")

    def IsInstalled(self) -> bool:
        """Check if cross toolchain and sysroot runtime are already installed.

        Returns:
            True if key toolchain components and musl sysroot archive are found.
        """
        RequiredTools = [
            self.bin_dir / f"{self.target}-as",
            self.bin_dir / f"{self.target}-gcc",
            self.sysroot / "usr" / "lib" / "libc.so",
            self.sysroot / "usr" / "include" / "linux" / "kernel.h",
        ]
        return all(Path.exists() for Path in RequiredTools)


# Main Entry Point


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
    Args = parser.parse_args()

    # Determine target triple
    if Args.target:
        Target = Args.target
    elif Args.arch:
        Target = GetArchConfig(Args.arch)["TargetTriple"]
    else:
        Target = GetArchConfig("i686")["TargetTriple"]

    Builder = ToolchainBuilder(
        prefix=Args.prefix,
        target=Target,
        jobs=Args.jobs,
    )

    try:
        if Args.clean_all:
            Builder.CleanAll()
        elif Args.clean:
            Builder.Clean()
        elif Args.binutils_only:
            Builder.SetupDirectories()
            Builder.DownloadSources()
            Builder.ExtractSources()
            Builder.BuildBinutils()
        elif Args.gcc_stage1_only:
            Builder.SetupDirectories()
            Builder.DownloadSources()
            Builder.ExtractSources()
            Builder.BuildGccStage1()
        else:
            Builder.BuildAll()
    except subprocess.CalledProcessError as Ex:
        print(
            f"\nError: Command failed with exit code {Ex.returncode}", file=sys.stderr
        )
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nBuild interrupted.")
        sys.exit(130)


if __name__ == "__main__":
    main()
