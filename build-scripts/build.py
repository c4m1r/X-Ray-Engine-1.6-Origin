#!/usr/bin/env python3
"""
Automated build for X-Ray SDK / engine.

Dependencies: Python 3, MSBuild (RAD + VS), 7-Zip.

Edit RAD_STUDIO_BIN and VS_INSTALL_DIR below (or set RAD_STUDIO_BIN / VSINSTALLDIR env vars).

RAD Studio group projects must use MSBuild platform Win64x. Visual Studio engine.sln uses Win64.

"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

RAD_STUDIO_BIN = Path(
    os.environ.get("RAD_STUDIO_BIN", r"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin")
)
VS_INSTALL_DIR = Path(
    os.environ.get("VSINSTALLDIR", r"C:\Program Files\Microsoft Visual Studio\18\Insiders")
)
SEVEN_ZIP_EXE = Path(os.environ.get("SEVEN_ZIP_EXE", r"C:\Program Files\7-Zip\7z.exe"))

RAD_MSBUILD_PLATFORM = "Win64x"

ARCHIVE_EXTENSIONS = frozenset({".dll", ".exe", ".mll", ".bpl", ".db_e"})
START_FOLDER = "start"

EDITORS_TOOLS_REL = Path("code") / "engine.vc2008" / "editors" / "tools"
PACK_EDITOR_RESOURCES_PS1 = EDITORS_TOOLS_REL / "pack_editor_splash.ps1"

REPO_CLEAN_DIRS = ("bins", "intermedia", "libraries")

BIN_FOLDER_VARIANTS = ("Win64", "Win64x") 


def clean_repo_build_trees(repo: Path, archive_path: Path) -> int:
    for name in REPO_CLEAN_DIRS:
        path = repo / name
        if not path.exists():
            continue
        q = str(path)
        print(f"+ rmtree {q}")
        try:
            shutil.rmtree(path)
        except OSError as e:
            print(f"ERROR: cannot remove {path}: {e}", file=sys.stderr)
            return 1
    if archive_path.is_file():
        q = str(archive_path)
        print(f"+ remove {q}")
        try:
            archive_path.unlink()
        except OSError as e:
            print(f"ERROR: cannot remove {archive_path}: {e}", file=sys.stderr)
            return 1
    return 0


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def build_scripts_dir() -> Path:
    return Path(__file__).resolve().parent


def rsvars_bat(rad_bin: Path) -> Path:
    return rad_bin / "rsvars.bat"


def vs_msbuild_exe(vs_install: Path) -> Path:
    return vs_install / "MSBuild" / "Current" / "Bin" / "MSBuild.exe"


def run_cmd(
    argv: list[str],
    *,
    cwd: Path | None = None,
) -> int:
    q = " ".join(f'"{a}"' if " " in a else a for a in argv)
    print(f"+ {q}")
    r = subprocess.run(argv, cwd=str(cwd) if cwd else None)
    return int(r.returncode)


def run_rad_msbuild(
    *,
    rsvars: Path,
    groupproj: Path,
    targets: str,
    config: str,
) -> int:
    cwd = groupproj.parent
    gp = groupproj.name
    inner = (
        f'call "{rsvars}" && '
        f'msbuild "{gp}" /t:{targets} '
        f"/p:Config={config} /p:Platform={RAD_MSBUILD_PLATFORM} "
        f"/m /v:minimal /nologo"
    )
    print(f"+ (rad) {inner}")
    r = subprocess.run(inner, shell=True, cwd=str(cwd))
    return int(r.returncode)


def build_components_prep(rsvars: Path, groupproj: Path) -> int:
    steps = [
        ("editorB", "Debug"),
        ("editorB", "Release"),
        ("editor", "Release"),
    ]
    for targets, cfg in steps:
        rc = run_rad_msbuild(
            rsvars=rsvars,
            groupproj=groupproj,
            targets=targets,
            config=cfg,
        )
        if rc != 0:
            print(f"ERROR: components step failed (target={targets}, Config={cfg})", file=sys.stderr)
            return rc
    return 0


def build_engine(msbuild: Path, sln: Path, configuration: str) -> int:
    argv = [
        str(msbuild),
        str(sln),
        "/m",
        "/t:xrEntry",
        f"/p:Configuration={configuration}",
        "/p:Platform=Win64",
        "/v:minimal",
        "/nologo",
    ]
    rc = run_cmd(argv, cwd=sln.parent)
    if rc != 0:
        print("ERROR: engine (xrEntry) build failed.", file=sys.stderr)
    return rc


def build_editors(
    rsvars: Path,
    groupproj: Path,
    configuration: str,
) -> int:
    return run_rad_msbuild(
        rsvars=rsvars,
        groupproj=groupproj,
        targets="ActorEditor;LevelEditor",
        config=configuration,
    )


def pack_editor_resources_for_config(repo: Path, configuration: str) -> int:
    script = repo / PACK_EDITOR_RESOURCES_PS1
    out_dir = repo / "bins" / "Win64" / configuration
    argv = [
        "powershell.exe",
        "-NoProfile",
        "-NonInteractive",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(script),
        "-OutputDir",
        str(out_dir.resolve()),
    ]
    q = " ".join(f'"{a}"' if " " in a else a for a in argv)
    print(f"+ pack_editor_resources ({configuration}): {q}")
    if not script.is_file():
        print(f"WARNING: resource pack script missing: {script}", file=sys.stderr)
        return 0
    r = subprocess.run(argv, cwd=str(repo))
    return int(r.returncode)


def collect_filtered_bin_paths(repo: Path, configurations: tuple[str, ...]) -> list[str]:
    rel: list[str] = []
    for cfg in configurations:
        for sub in BIN_FOLDER_VARIANTS:
            base = repo / "bins" / sub / cfg
            if not base.is_dir():
                continue
            for p in sorted(base.rglob("*")):
                if p.is_file() and p.suffix.lower() in ARCHIVE_EXTENSIONS:
                    rel.append(str(p.relative_to(repo)).replace("/", "\\"))
    return sorted(set(rel))


def collect_start_script_paths(repo: Path, configurations: tuple[str, ...]) -> list[str]:
    start_dir = repo / START_FOLDER
    if not start_dir.is_dir():
        return []
    needles = tuple(f"_Win64_{cfg}" for cfg in configurations)
    rel: list[str] = []
    for p in sorted(start_dir.iterdir()):
        if not p.is_file():
            continue
        if p.suffix.lower() != ".bat":
            continue
        name = p.name
        if any(n in name for n in needles):
            rel.append(str(p.relative_to(repo)).replace("/", "\\"))
    return sorted(rel)


def pack_bins_archive(
    repo: Path,
    configurations: tuple[str, ...],
    archive_path: Path,
    seven_zip: Path,
) -> int:
    bins_paths = collect_filtered_bin_paths(repo, configurations)
    start_paths = collect_start_script_paths(repo, configurations)
    rel = sorted(set(bins_paths + start_paths))

    if not bins_paths:
        print(
            f"ERROR: no files matching {sorted(ARCHIVE_EXTENSIONS)} under bins\\Win64\\ or bins\\Win64x\\ "
            f"for configs {list(configurations)}",
            file=sys.stderr,
        )
        return 1

    bins_set = set(bins_paths)

    list_file = archive_path.parent / f"_7z_list_{archive_path.stem}.txt"
    print(
        f"+ [archive] {archive_path.name}: {len(bins_paths)} under bins\\{{Win64|Win64x}}\\..., "
        f"{len(start_paths)} under {START_FOLDER}\\ ({'+'.join(configurations)}) — {len(rel)} paths listed"
    )

    present_posix: list[str] = []
    missing: list[str] = []
    for rw in rel:
        full = repo / Path(rw.replace("\\", "/"))
        if full.is_file():
            present_posix.append(full.relative_to(repo).as_posix())
        else:
            missing.append(rw.replace("/", "\\"))

    missing_bins = [m for m in missing if m in bins_set]
    if missing_bins:
        sample = missing_bins[:30]
        more = "" if len(missing_bins) <= 30 else f" … (+{len(missing_bins) - 30} more)"
        print(
            "ERROR: bins paths listed for archive but missing on disk (7-Zip would skip them): "
            + "; ".join(sample)
            + more,
            file=sys.stderr,
        )
        print(
            "Hint: run a full build first; outputs must exist under bins before 7-Zip.",
            file=sys.stderr,
        )
        return 1

    missing_optional = [m for m in missing if m not in bins_set]
    for m in missing_optional:
        print(f"WARNING: skip missing optional path: {m}", file=sys.stderr)

    list_file.write_text("\n".join(present_posix) + "\n", encoding="utf-8", newline="\n")
    argv = [
        str(seven_zip),
        "a",
        "-t7z",
        "-mx=7",
        "-scsUTF-8",
        str(archive_path.resolve()),
        f"@{list_file.resolve()}",
    ]
    r = subprocess.run(argv, cwd=str(repo))
    try:
        list_file.unlink(missing_ok=True)
    except OSError:
        pass
    if r.returncode != 0:
        print(f"ERROR: 7z failed ({archive_path})", file=sys.stderr)
        return int(r.returncode)
    print(f"Created: {archive_path} ({len(present_posix)} files)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="X-Ray Engine / SDK batch build.")
    mx = ap.add_mutually_exclusive_group(required=True)
    mx.add_argument(
        "--config",
        choices=("Debug", "Release"),
        help="Single configuration for VS/RAD steps and single archive (Debug.7z or Release.7z).",
    )
    mx.add_argument(
        "--full",
        action="store_true",
        help="Build Debug and Release (VS + RAD); archive Full.7z with both bins trees.",
    )
    ap.add_argument(
        "--rad-studio-bin",
        type=Path,
        metavar="DIR",
        default=RAD_STUDIO_BIN,
        help=f"RAD Studio bin directory (contains rsvars.bat). Default: {RAD_STUDIO_BIN}",
    )
    ap.add_argument(
        "--vs-install",
        type=Path,
        metavar="DIR",
        default=VS_INSTALL_DIR,
        help=f"Visual Studio installation root. Default: {VS_INSTALL_DIR}",
    )
    ap.add_argument(
        "--seven-zip",
        type=Path,
        metavar="PATH",
        dest="seven_zip_exe",
        default=SEVEN_ZIP_EXE,
        help=f"Path to 7z.exe. Default: {SEVEN_ZIP_EXE}",
    )
    args = ap.parse_args()

    root = repo_root()
    scripts = build_scripts_dir()
    components_gp = root / "code" / "SDK" / "components" / "components.groupproj"
    engine_sln = root / "code" / "engine.vc2008" / "engine.sln"
    editors_gp = root / "code" / "engine.vc2008" / "all_editors.groupproj"

    for label, p in (
        ("components.groupproj", components_gp),
        ("engine.sln", engine_sln),
        ("all_editors.groupproj", editors_gp),
    ):
        if not p.is_file():
            print(f"ERROR: missing file {label}: {p}", file=sys.stderr)
            return 2

    rsvars = rsvars_bat(args.rad_studio_bin)
    if not rsvars.is_file():
        print(f"ERROR: rsvars.bat not found: {rsvars}", file=sys.stderr)
        return 2

    msbuild = vs_msbuild_exe(args.vs_install)
    if not msbuild.is_file():
        print(f"ERROR: MSBuild.exe not found: {msbuild}", file=sys.stderr)
        return 2

    if args.full:
        configs: tuple[str, ...] = ("Debug", "Release")
        archive_path = scripts / "Full.7z"
    else:
        cfg = args.config
        configs = (cfg,)
        archive_path = scripts / f"{cfg}.7z"

    print(f"=== [1/5] Clean repo dirs: {', '.join(REPO_CLEAN_DIRS)} ===")
    rc = clean_repo_build_trees(root, archive_path)
    if rc != 0:
        return rc

    print(f"=== [2/5] RAD Studio: components ({RAD_MSBUILD_PLATFORM}) ===")
    rc = build_components_prep(rsvars, components_gp)
    if rc != 0:
        return rc

    label_cfgs = " + ".join(configs) if args.full else configs[0]
    print(f"=== [3/5] Visual Studio: xrEntry ({label_cfgs}|Win64) ===")
    for cfg in configs:
        rc = build_engine(msbuild, engine_sln, cfg)
        if rc != 0:
            return rc

    print(f"=== [4/5] RAD Studio: ActorEditor + LevelEditor ({label_cfgs}|{RAD_MSBUILD_PLATFORM}) ===")
    for cfg in configs:
        rc = build_editors(rsvars, editors_gp, cfg)
        if rc != 0:
            return rc
        rc = pack_editor_resources_for_config(root, cfg)
        if rc != 0:
            print(f"ERROR: pack_editor_resources failed (Config={cfg}).", file=sys.stderr)
            return rc

    arc_label = archive_path.name
    print(f"=== [5/5] 7-Zip — {arc_label} (bins + start\\*.bat by configuration) ===")
    sz = args.seven_zip_exe
    if not sz.is_file():
        print(f"ERROR: 7z not found: {sz}", file=sys.stderr)
        return 2
    rc = pack_bins_archive(root, configs, archive_path, sz)
    if rc != 0:
        return rc

    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
