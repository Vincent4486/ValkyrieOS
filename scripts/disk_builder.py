#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import shlex

SectorSize = 512
Stage2LoadAddr = 0x7E00
CorefsLoadAddr = 0x57E00
DefaultLabel = "VALECIUM"


class DiskBuildError(RuntimeError):
    pass


def RunCommand(Args, InputText=None, **Kwargs):
    subprocess.run(
        Args,
        check=True,
        input=InputText,
        text=InputText is not None,
        **Kwargs,
    )


def RequireTool(Name):
    if shutil.which(Name) is None:
        raise DiskBuildError(f"Missing required tool: {Name}")


def ParseSize(Value):
    Value = Value.strip()
    if not Value:
        raise DiskBuildError("Image size is empty")

    Suffix = Value[-1].upper()
    if Suffix in ("K", "M", "G", "T"):
        Number = Value[:-1]
        if not Number:
            raise DiskBuildError(f"Invalid size: {Value}")
        Multiplier = {
            "K": 1024,
            "M": 1024 * 1024,
            "G": 1024 * 1024 * 1024,
            "T": 1024 * 1024 * 1024 * 1024,
        }[Suffix]
        return int(Number) * Multiplier

    return int(Value)


def AlignUp(Value, Align):
    return (Value + Align - 1) // Align * Align


def GetDirSize(Path):
    Total = 0
    for Root, _, Files in os.walk(Path):
        for Name in Files:
            Full = os.path.join(Root, Name)
            try:
                Total += os.lstat(Full).st_size
            except FileNotFoundError:
                continue
    return Total


def PatchStage1(Stage1Bytes, Stage2Lba, Stage2Sectors):
    Sig = b"VLSP"
    Idx = Stage1Bytes.find(Sig)
    if Idx == -1:
        raise DiskBuildError("Stage1 signature VLSP not found")
    if Stage1Bytes.find(Sig, Idx + 1) != -1:
        raise DiskBuildError("Stage1 signature VLSP appears multiple times")

    Data = bytearray(Stage1Bytes)
    struct.pack_into("<I", Data, Idx + 4, Stage2Lba)
    struct.pack_into("<H", Data, Idx + 8, Stage2Sectors)
    return bytes(Data)


def PatchStage2(Stage2Bytes, LabelBytes, UuidBytes):
    Sig = b"VLSF"
    Idx = Stage2Bytes.find(Sig)
    if Idx == -1:
        raise DiskBuildError("Stage2 signature VLSF not found")
    if Stage2Bytes.find(Sig, Idx + 1) != -1:
        raise DiskBuildError("Stage2 signature VLSF appears multiple times")

    Data = bytearray(Stage2Bytes)
    struct.pack_into("<I", Data, Idx + 4, CorefsLoadAddr)
    Data[Idx + 8 : Idx + 8 + 32] = LabelBytes
    Data[Idx + 40 : Idx + 40 + 16] = UuidBytes
    return bytes(Data)


def BuildLabelBytes(Label):
    if not Label:
        return b"\x00" * 32
    Encoded = Label.encode("ascii", errors="replace")
    return Encoded[:32].ljust(32, b"\x00")


def ParseUuidBytes(UuidStr):
    if not UuidStr:
        return None
    Compact = UuidStr.replace("-", "").strip()
    if not Compact or len(Compact) > 32:
        return None
    if len(Compact) % 2 == 1:
        Compact = Compact + "0"
    try:
        Raw = bytes.fromhex(Compact)
        return Raw.ljust(16, b"\x00")
    except ValueError:
        return None


def BuildStage2Blob(Stage2Bytes, CorefsBytes):
    if CorefsBytes is None:
        Combined = Stage2Bytes
    else:
        CorefsOffset = CorefsLoadAddr - Stage2LoadAddr
        if CorefsOffset < len(Stage2Bytes):
            raise DiskBuildError(
                "Stage2 is larger than the corefs load offset; refuse to append corefs."
            )
        Padding = CorefsOffset - len(Stage2Bytes)
        Combined = Stage2Bytes + (b"\x00" * Padding) + CorefsBytes

    Sectors = (len(Combined) + SectorSize - 1) // SectorSize
    Total = Sectors * SectorSize
    Combined = Combined.ljust(Total, b"\x00")
    return Combined, Sectors


def WriteAt(Path, Offset, Data):
    with open(Path, "r+b") as Handle:
        Handle.seek(Offset)
        Handle.write(Data)
        Handle.flush()
        os.fsync(Handle.fileno())


def ResolveLinkSource(LinkPath, StagingRoot):
    """Resolve a symlink target relative to the StagingRoot.

    If the symlink target starts with ``/``, the ``/`` is treated as the
    StagingRoot directory.  Relative targets are resolved from the
    symlink's parent directory.  The caller is responsible for checking
    that the returned path exists.
    """
    Target = os.readlink(LinkPath)
    if Target.startswith("/"):
        Resolved = os.path.normpath(os.path.join(StagingRoot, Target.lstrip("/")))
    else:
        Resolved = os.path.normpath(os.path.join(os.path.dirname(LinkPath), Target))
    return Resolved


def CopyLink(LinkPath, StagingRoot, DstPath):
    """Copy the content that *LinkPath* points to to *DstPath*.

    Symlinks cannot be created on the target filesystem (e.g. vfat), so
    the actual file or directory tree is copied instead.
    """
    Resolved = ResolveLinkSource(LinkPath, StagingRoot)
    if not os.path.lexists(Resolved):
        raise DiskBuildError(f"Symlink target does not exist: {LinkPath} -> {Resolved}")
    if os.path.isdir(Resolved):
        os.makedirs(DstPath, exist_ok=True)
        CopyTreeContents(Resolved, DstPath)
    else:
        shutil.copy2(Resolved, DstPath, follow_symlinks=False)


def CopyTreeContents(Src, Dst):
    for Root, Dirs, Files in os.walk(Src):
        Rel = os.path.relpath(Root, Src)
        TargetRoot = Dst if Rel == "." else os.path.join(Dst, Rel)
        os.makedirs(TargetRoot, exist_ok=True)
        shutil.copystat(Root, TargetRoot, follow_symlinks=False)
        for Name in Dirs:
            SrcPath = os.path.join(Root, Name)
            DstPath = os.path.join(TargetRoot, Name)
            if os.path.islink(SrcPath):
                if os.path.lexists(DstPath):
                    os.remove(DstPath)
                CopyLink(SrcPath, Src, DstPath)
            else:
                os.makedirs(DstPath, exist_ok=True)
                shutil.copystat(SrcPath, DstPath, follow_symlinks=False)
        for Name in Files:
            SrcPath = os.path.join(Root, Name)
            DstPath = os.path.join(TargetRoot, Name)
            if os.path.islink(SrcPath):
                if os.path.lexists(DstPath):
                    os.remove(DstPath)
                CopyLink(SrcPath, Src, DstPath)
            else:
                shutil.copy2(SrcPath, DstPath, follow_symlinks=False)


def WaitForPartition(LoopDev):
    Candidates = [f"{LoopDev}p1", f"{LoopDev}1"]
    for _ in range(50):
        for Candidate in Candidates:
            if os.path.exists(Candidate):
                return Candidate
        RunCommand(["udevadm", "settle"], stdout=subprocess.DEVNULL)
        time.sleep(0.1)
    raise DiskBuildError("Partition device did not appear for loopback")


def PartitionMbr(ImagePath, StartLba, TotalSectors):
    EndLba = TotalSectors - 1
    if EndLba <= StartLba:
        raise DiskBuildError("Partition size is invalid")

    RunCommand(["parted", "-s", ImagePath, "mklabel", "msdos"])
    RunCommand(
        [
            "parted",
            "-s",
            "-a",
            "minimal",
            ImagePath,
            "mkpart",
            "primary",
            f"{StartLba}s",
            f"{EndLba}s",
        ]
    )
    RunCommand(["parted", "-s", ImagePath, "set", "1", "boot", "on"])


def PartitionGpt(ImagePath, StartLba, TotalSectors, Label):
    if not shutil.which("parted"):
        raise DiskBuildError("GPT requires parted")

    EndLba = TotalSectors - 34
    if EndLba <= StartLba:
        raise DiskBuildError("Image size is too small for GPT partition")

    RunCommand(["parted", "-s", ImagePath, "mklabel", "gpt"])
    RunCommand(
        [
            "parted",
            "-s",
            "-a",
            "minimal",
            ImagePath,
            "mkpart",
            "primary",
            f"{StartLba}s",
            f"{EndLba}s",
        ]
    )
    RunCommand(["parted", "-s", ImagePath, "name", "1", Label])


def MkfsLabelArgs(FsType, Label):
    if not Label:
        return []
    Mapping = {
        "ext2": ["-L", Label],
        "ext3": ["-L", Label],
        "ext4": ["-L", Label],
        "xfs": ["-L", Label],
        "btrfs": ["-L", Label],
        "vfat": ["-n", Label],
        "fat": ["-n", Label],
        "msdos": ["-n", Label],
        "exfat": ["-n", Label],
        "f2fs": ["-l", Label],
    }
    return Mapping.get(FsType, [])


def ReadBlkid(PartDev):
    Result = subprocess.run(
        ["blkid", "-o", "export", PartDev],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if Result.returncode != 0:
        return None, None
    Label = None
    Uuid = None
    for Line in Result.stdout.splitlines():
        if Line.startswith("LABEL="):
            Label = Line.split("=", 1)[1].strip()
        elif Line.startswith("UUID="):
            Uuid = Line.split("=", 1)[1].strip()
    return Label, Uuid


def main():
    parser = argparse.ArgumentParser(
        description="Build a bootable disk image for the Valecium bootloader."
    )
    parser.add_argument("--stage1", required=True, help="Path to stage1 boot.bin")
    parser.add_argument("--stage2", required=True, help="Path to stage2 core.bin")
    parser.add_argument(
        "--staging",
        required=True,
        help="Directory containing files to copy into the image",
    )
    parser.add_argument("--output", required=True, help="Output disk image path")
    parser.add_argument("--fs", required=True, help="Core filesystem module name (e.g. ext2, fat)")
    parser.add_argument(
        "--mkfs",
        required=True,
        help="Full mkfs command (e.g. mkfs.ext4, mkfs.fat)",
    )
    parser.add_argument(
        "--scheme",
        required=True,
        choices=["mbr", "gpt"],
        help="Partition scheme",
    )
    parser.add_argument("--label", default=DefaultLabel, help="Filesystem label")
    parser.add_argument("--image-size", help="Image size (bytes or with K/M/G)")
    parser.add_argument(
        "--partition-start",
        type=int,
        default=2048,
        help="Partition start LBA (default: 2048)",
    )
    parser.add_argument(
        "--stage2-start",
        type=int,
        help="Stage2 start LBA (defaults to 1 for MBR, 34 for GPT)",
    )
    parser.add_argument(
        "--corefs",
        help="Optional corefs_<fs>.bin path (default: auto-detect)",
    )

    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing output image",
    )

    Args = parser.parse_args()

    if not sys.platform.startswith("linux"):
        raise DiskBuildError("This script only supports Linux")
    if os.geteuid() != 0:
        raise DiskBuildError("Run this script as root (sudo)")

    Stage1Path = os.path.abspath(Args.stage1)
    Stage2Path = os.path.abspath(Args.stage2)
    StagingDir = os.path.abspath(Args.staging)
    OutputPath = os.path.abspath(Args.output)

    if not os.path.isfile(Stage1Path):
        raise DiskBuildError(f"Stage1 not found: {Stage1Path}")
    if not os.path.isfile(Stage2Path):
        raise DiskBuildError(f"Stage2 not found: {Stage2Path}")
    if not os.path.isdir(StagingDir):
        raise DiskBuildError(f"Staging dir not found: {StagingDir}")

    RequireTool("losetup")
    RequireTool("parted")
    RequireTool("mount")
    RequireTool("umount")
    RequireTool("blkid")
    RequireTool("udevadm")
    RequireTool("partprobe")
    RequireTool(Args.mkfs.split()[0])

    if Args.scheme == "gpt":
        if not shutil.which("parted"):
            raise DiskBuildError("GPT requires parted")

    if os.path.exists(OutputPath):
        if Args.force:
            os.remove(OutputPath)
        else:
            raise DiskBuildError(f"Output already exists: {OutputPath} (use --force)")

    with open(Stage1Path, "rb") as Handle:
        Stage1Bytes = Handle.read()
    with open(Stage2Path, "rb") as Handle:
        Stage2Base = Handle.read()

    Stage2Start = Args.stage2_start
    if Stage2Start is None:
        Stage2Start = 1 if Args.scheme == "mbr" else 34

    if Stage2Start <= 0:
        raise DiskBuildError("Stage2 start LBA must be >= 1")
    if Args.scheme == "gpt" and Stage2Start < 34:
        raise DiskBuildError("GPT stage2 start must be >= 34")

    CorefsBytes = None
    CorefsPath = None
    if Args.corefs:
        CorefsPath = os.path.abspath(Args.corefs)
        if not os.path.isfile(CorefsPath):
            raise DiskBuildError(f"Corefs not found: {CorefsPath}")
        with open(CorefsPath, "rb") as Handle:
            CorefsBytes = Handle.read()
    else:
        Guessed = os.path.join(os.path.dirname(Stage2Path), f"corefs_{Args.fs}.bin")
        if os.path.isfile(Guessed):
            CorefsPath = Guessed
            with open(CorefsPath, "rb") as Handle:
                CorefsBytes = Handle.read()
        else:
            print(
                f"Warning: corefs_{Args.fs}.bin not found; stage2 will not "
                "include a filesystem driver.",
                file=sys.stderr,
            )

    if CorefsBytes is not None:
        CorefsOffset = CorefsLoadAddr - Stage2LoadAddr
        if len(Stage2Base) >= CorefsOffset:
            print(
                "Warning: Stage2 is already at/after the corefs offset; "
                "skipping corefs append.",
                file=sys.stderr,
            )
            CorefsBytes = None

    _, Stage2Sectors = BuildStage2Blob(Stage2Base, CorefsBytes)

    if Stage2Start + Stage2Sectors > Args.partition_start:
        raise DiskBuildError(
            "Stage2 overlaps the main partition; adjust start LBAs or size"
        )

    StagingSize = GetDirSize(StagingDir)
    MinSize = Args.partition_start * SectorSize + StagingSize + 64 * 1024 * 1024
    if Args.image_size:
        ImageSize = ParseSize(Args.image_size)
        if ImageSize < MinSize:
            raise DiskBuildError("Image size is too small for staging content")
    else:
        ImageSize = AlignUp(MinSize, 1024 * 1024)

    TotalSectors = ImageSize // SectorSize
    if TotalSectors <= Args.partition_start:
        raise DiskBuildError("Image size does not allow a partition")

    with open(OutputPath, "wb") as Handle:
        Handle.truncate(ImageSize)

    if Args.scheme == "mbr":
        PartitionMbr(OutputPath, Args.partition_start, TotalSectors)
    else:
        PartitionGpt(OutputPath, Args.partition_start, TotalSectors, Args.label)

    Stage1Patched = PatchStage1(Stage1Bytes, Stage2Start, Stage2Sectors)
    if len(Stage1Patched) > 0x1BE:
        raise DiskBuildError(f"Stage1 size exceeds 0x1BE bytes: {len(Stage1Patched)}")
    Stage1Patched = Stage1Patched.ljust(0x1BE, b"\x00")
    WriteAt(OutputPath, 0, Stage1Patched)

    LoopDev = None
    MountDir = None
    PartDev = None

    try:
        LoopDev = subprocess.check_output(
            ["losetup", "--find", "--show", "--partscan", OutputPath],
            text=True,
        ).strip()
        RunCommand(["partprobe", LoopDev], stdout=subprocess.DEVNULL)
        PartDev = WaitForPartition(LoopDev)

        MkfsCmd = shlex.split(Args.mkfs)
        MkfsCmd.extend(MkfsLabelArgs(Args.fs, Args.label))
        MkfsCmd.append(PartDev)
        RunCommand(MkfsCmd)

        FsLabel, FsUuid = ReadBlkid(PartDev)
        if not FsLabel:
            FsLabel = Args.label
            if Args.fs in ("vfat", "fat", "msdos", "exfat"):
                FsLabel = FsLabel.upper()
        UuidBytes = ParseUuidBytes(FsUuid)
        if UuidBytes is None:
            UuidBytes = b"\x00" * 16

        LabelBytes = BuildLabelBytes(FsLabel)
        Stage2Patched = PatchStage2(Stage2Base, LabelBytes, UuidBytes)
        Stage2Blob, _ = BuildStage2Blob(Stage2Patched, CorefsBytes)
        WriteAt(OutputPath, Stage2Start * SectorSize, Stage2Blob)

        MountDir = tempfile.mkdtemp(prefix="valecium-img-")
        RunCommand(["mount", PartDev, MountDir])
        CopyTreeContents(StagingDir, MountDir)
        RunCommand(["sync"])
        RunCommand(["umount", MountDir])
        shutil.rmtree(MountDir, ignore_errors=True)
        MountDir = None
    finally:
        if MountDir:
            try:
                RunCommand(["umount", MountDir])
            except Exception:
                pass
            shutil.rmtree(MountDir, ignore_errors=True)
        if LoopDev:
            try:
                RunCommand(["losetup", "-d", LoopDev])
            except Exception:
                pass

    print("Disk image written:", OutputPath)
    print("Stage2 LBA:", Stage2Start)
    print("Stage2 sectors:", Stage2Sectors)


if __name__ == "__main__":
    try:
        main()
    except DiskBuildError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as exc:
        print(f"error: command failed: {exc}", file=sys.stderr)
        sys.exit(exc.returncode)
