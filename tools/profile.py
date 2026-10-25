#!/usr/bin/env python3
"""Isolated SkyCRPG test profiles for a Steam/Proton Skyrim SE install.

A profile is a staging directory outside the repo and outside Steam:

    <profile-root>/<profile>/saves/   copied fixture saves (owner originals untouched)
    <profile-root>/<profile>/runs/    collected logs, one directory per launch
    <profile-root>/active.json        manifest of every file currently deployed

`deploy` copies SKSE and the profile's SkyCRPG archives into the game install,
refusing to overwrite any file it does not own, and points SLocalSavePath at a
per-profile save directory outside the Steam Cloud "Saves" folder. `undeploy`
removes exactly what the manifest lists, after checking hashes. Only one
profile can be deployed at a time, and `launch` refuses to start while any
SkyrimSE.exe is running.

    tools/profile.py init <profile> [--save NAME ...]
    tools/profile.py deploy <profile> --skse <extracted skse64_2_03_01 dir> [--dist dist/]
    tools/profile.py launch [--until-loaded | --interactive] [--timeout SECONDS]
    tools/profile.py undeploy
    tools/profile.py status
"""

import argparse
import hashlib
import json
import os
import shutil
import signal
import subprocess
import sys
import time
import uuid
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROFILES = json.loads((ROOT / "profiles" / "profiles.json").read_text())

STEAM = Path(os.environ.get("STEAM_ROOT", Path.home() / ".local/share/Steam"))
GAME = Path(os.environ.get("SKYRIM_DIR", STEAM / "steamapps/common/Skyrim Special Edition"))
COMPAT = Path(os.environ.get("SKYRIM_COMPATDATA", STEAM / "steamapps/compatdata/489830"))
PROTON = Path(os.environ.get("PROTON_DIR", STEAM / "steamapps/common/Proton - Experimental"))
RUNTIME = Path(os.environ.get("STEAM_RUNTIME_DIR", STEAM / "steamapps/common/SteamLinuxRuntime_4"))
PROFILE_ROOT = Path(os.environ.get("SKYCRPG_PROFILE_ROOT", "/srv/storage/Projects/SkyCRPG/profiles"))

MY_GAMES = COMPAT / "pfx/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition"
OWNER_SAVES = MY_GAMES / "Saves"  # Steam Cloud root; read-only for this tool
SKSE_LOGS = MY_GAMES / "SKSE"
CUSTOM_INI = MY_GAMES / "SkyrimCustom.ini"
ACTIVE = PROFILE_ROOT / "active.json"
EXPECTED_EXE_SHA256 = "846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f"  # 1.7.104.0


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fail(message: str) -> None:
    sys.exit(f"error: {message}")


def profile_dir(name: str) -> Path:
    if name not in PROFILES:
        fail(f"unknown profile {name!r}; choose from {', '.join(PROFILES)}")
    return PROFILE_ROOT / name


def save_dir_relative(name: str) -> str:
    return f"SkyCRPG\\{name}\\Saves\\"


def load_active() -> dict | None:
    return json.loads(ACTIVE.read_text()) if ACTIVE.exists() else None


def skyrim_pids() -> list[int]:
    out = subprocess.run(["pgrep", "-f", "SkyrimSE.exe"], capture_output=True, text=True).stdout
    return [int(p) for p in out.split() if int(p) != os.getpid()]


def cmd_init(args) -> None:
    pdir = profile_dir(args.profile)
    (pdir / "saves").mkdir(parents=True, exist_ok=True)
    (pdir / "runs").mkdir(exist_ok=True)
    copied = []
    for name in args.save:
        src = OWNER_SAVES / name
        if not src.is_file():
            fail(f"save not found: {src}")
        for suffix in (".ess", ".skse"):
            extra = src.with_suffix(suffix)
            if extra.is_file():
                shutil.copy2(extra, pdir / "saves" / extra.name)
        copied.append({"file": name, "sha256": sha256(src)})
    fixtures = pdir / "fixtures.json"
    existing = json.loads(fixtures.read_text()) if fixtures.exists() else []
    known = {f["file"] for f in existing}
    fixtures.write_text(json.dumps(existing + [c for c in copied if c["file"] not in known], indent=2) + "\n")
    print(f"{args.profile}: {pdir} ({len(list((pdir / 'saves').glob('*.ess')))} saves)")


def plan_files(profile: str, skse_dir: Path, dist: Path) -> dict[Path, bytes]:
    """Map destination path -> content for the profile's prerequisites and packages."""
    files: dict[Path, bytes] = {}
    loader = skse_dir / "skse64_loader.exe"
    if not loader.is_file():
        fail(f"{loader} missing; pass the extracted SKSE64 2.3.1 directory")
    for src in sorted(skse_dir.rglob("*")):
        rel = src.relative_to(skse_dir)
        if src.is_file() and (rel.parts[0] == "Data" or (len(rel.parts) == 1 and src.suffix in (".exe", ".dll"))):
            files[GAME / rel] = src.read_bytes()
    for package in PROFILES[profile]["packages"]:
        archives = sorted(dist.glob(f"{package}-*.zip"))
        if len(archives) != 1:
            fail(f"expected exactly one {package}-*.zip in {dist}, found {len(archives)}")
        with zipfile.ZipFile(archives[0]) as z:
            for entry in z.namelist():
                if not entry.endswith("/"):
                    files[GAME / "Data" / entry] = z.read(entry)
    return files


def cmd_deploy(args) -> None:
    if load_active():
        fail(f"profile {load_active()['profile']!r} is deployed; run undeploy first")
    if skyrim_pids():
        fail("SkyrimSE.exe is running")
    exe_hash = sha256(GAME / "SkyrimSE.exe")
    if exe_hash != EXPECTED_EXE_SHA256:
        fail(f"SkyrimSE.exe sha256 {exe_hash} is not the recorded 1.7.104.0 executable")
    pdir = profile_dir(args.profile)
    if not pdir.exists():
        fail(f"run init {args.profile} first")
    if CUSTOM_INI.exists():
        fail(f"{CUSTOM_INI} already exists and is not owned by this tool")

    files = plan_files(args.profile, args.skse.resolve(), args.dist.resolve())
    for dest, data in files.items():
        if dest.exists() and dest.read_bytes() != data:
            fail(f"{dest} exists with different content; refusing to overwrite")

    manifest = {"profile": args.profile, "deployed_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                "exe_sha256": exe_hash, "files": [], "dirs": [], "custom_ini": str(CUSTOM_INI)}
    ACTIVE.parent.mkdir(parents=True, exist_ok=True)
    ACTIVE.write_text(json.dumps(manifest, indent=2))  # written first so a partial deploy can be undone
    for dest, data in sorted(files.items()):
        for parent in reversed(dest.parents):
            if not parent.exists():
                parent.mkdir()
                manifest["dirs"].append(str(parent))
        preexisting = dest.exists()
        if not preexisting:
            dest.write_bytes(data)
        manifest["files"].append({"path": str(dest), "sha256": hashlib.sha256(data).hexdigest(), "preexisting": preexisting})
        ACTIVE.write_text(json.dumps(manifest, indent=2))

    saves = MY_GAMES / "SkyCRPG" / args.profile / "Saves"
    saves.mkdir(parents=True, exist_ok=True)
    for ess in (pdir / "saves").iterdir():
        if not (saves / ess.name).exists():
            shutil.copy2(ess, saves / ess.name)
    CUSTOM_INI.write_text(f"[General]\r\nSLocalSavePath={save_dir_relative(args.profile)}\r\n")
    ACTIVE.write_text(json.dumps(manifest, indent=2))
    print(f"deployed {args.profile}: {len(files)} files, saves in {saves}")


def cmd_undeploy(args) -> None:
    manifest = load_active()
    if not manifest:
        print("nothing deployed")
        return
    if skyrim_pids():
        fail("SkyrimSE.exe is running")
    for entry in reversed(manifest["files"]):
        path = Path(entry["path"])
        if entry["preexisting"] or not path.exists():
            continue
        if sha256(path) != entry["sha256"]:
            fail(f"{path} changed since deploy; inspect it before removing")
        path.unlink()
    for d in reversed(manifest["dirs"]):
        try:
            Path(d).rmdir()
        except OSError:
            print(f"kept non-empty directory {d}")
    custom = Path(manifest["custom_ini"])
    if custom.exists() and "SLocalSavePath=SkyCRPG\\" in custom.read_text():
        custom.unlink()
    ACTIVE.unlink()
    print(f"undeployed {manifest['profile']}; profile saves remain in {MY_GAMES / 'SkyCRPG' / manifest['profile']}")


def tagged_pids(tag: str) -> list[int]:
    pids = []
    for proc in Path("/proc").iterdir():
        if proc.name.isdigit():
            try:
                if f"SKYCRPG_RUN_ID={tag}".encode() in (proc / "environ").read_bytes():
                    pids.append(int(proc.name))
            except OSError:
                pass
    return pids


def stop_tagged(tag: str) -> list[int]:
    """Stop only processes started by this launch, identified by their environment tag."""
    for sig in (signal.SIGTERM, signal.SIGKILL):
        pids = tagged_pids(tag)
        for pid in pids:
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                pass
        deadline = time.time() + 15
        while tagged_pids(tag) and time.time() < deadline:
            time.sleep(1)
    return tagged_pids(tag)


def expected_logs(profile: str) -> list[str]:
    return ["skse64.log"] + [f"{p}.log" for p in PROFILES[profile]["packages"]]


def collect(profile: str, run_dir: Path, started: float) -> dict:
    run_dir.mkdir(parents=True, exist_ok=True)
    found = {}
    for name in ["skse64.log", "skse64_loader.log"] + [f"{p}.log" for p in PROFILES]:
        path = SKSE_LOGS / name
        if path.exists() and path.stat().st_mtime >= started:
            shutil.copy2(path, run_dir / name)
            found[name] = sha256(path)
    return found


def cmd_launch(args) -> None:
    manifest = load_active()
    if not manifest:
        fail("deploy a profile first")
    if skyrim_pids():
        fail("SkyrimSE.exe is already running; one Skyrim process per profile")
    profile = manifest["profile"]
    tag = uuid.uuid4().hex
    started = time.time()
    run_dir = profile_dir(profile) / "runs" / time.strftime("%Y%m%dT%H%M%S")
    run_dir.mkdir(parents=True)
    env = dict(os.environ,
               SKYCRPG_RUN_ID=tag,
               SteamAppId="489830", SteamGameId="489830", STEAM_COMPAT_APP_ID="489830",
               STEAM_COMPAT_DATA_PATH=str(COMPAT),
               STEAM_COMPAT_CLIENT_INSTALL_PATH=str(STEAM),
               STEAM_COMPAT_INSTALL_PATH=str(GAME),
               PROTON_LOG="1", PROTON_LOG_DIR=str(run_dir))
    command = [str(RUNTIME / "_v2-entry-point"), "--verb=waitforexitandrun", "--",
               str(PROTON / "proton"), "waitforexitandrun", str(GAME / "skse64_loader.exe")]
    log = open(run_dir / "launch.log", "w")
    proc = subprocess.Popen(command, cwd=GAME, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    record = {"profile": profile, "run_id": tag, "pid": proc.pid, "command": command,
              "started": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "timeout_s": args.timeout,
              "mode": "interactive" if args.interactive else "until-loaded"}
    (run_dir / "run.json").write_text(json.dumps(record, indent=2))
    print(f"launched {profile}: pid {proc.pid}, run {run_dir}, timeout {args.timeout}s", flush=True)

    wanted = expected_logs(profile)
    result = "TIMEOUT"
    deadline = started + args.timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            result = "EXITED"
            break
        fresh = [n for n in wanted if (SKSE_LOGS / n).exists() and (SKSE_LOGS / n).stat().st_mtime >= started]
        if not args.interactive and len(fresh) == len(wanted) and skyrim_pids():
            time.sleep(args.settle)  # let SKSE finish writing plugin results
            result = "LOADED"
            break
        time.sleep(2)

    leftover = stop_tagged(tag) if result != "EXITED" or tagged_pids(tag) else []
    record.update(result=result, ended=time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                  logs=collect(profile, run_dir, started), leftover_pids=leftover,
                  skyrim_running_after=skyrim_pids())
    (run_dir / "run.json").write_text(json.dumps(record, indent=2))
    print(json.dumps({k: record[k] for k in ("result", "logs", "leftover_pids", "skyrim_running_after")}, indent=2))
    if leftover:
        fail(f"processes from this run still alive: {leftover}")


def cmd_status(args) -> None:
    manifest = load_active()
    print(f"deployed: {manifest['profile'] if manifest else 'none'}")
    print(f"SkyrimSE.exe running: {skyrim_pids() or 'no'}")
    for name in PROFILES:
        pdir = PROFILE_ROOT / name
        if pdir.exists():
            print(f"{name}: {len(list((pdir / 'saves').glob('*.ess')))} saves, {len(list((pdir / 'runs').iterdir()))} runs")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("init")
    p.add_argument("profile")
    p.add_argument("--save", action="append", default=[], help="owner save file name to copy (repeatable)")
    p.set_defaults(func=cmd_init)
    p = sub.add_parser("deploy")
    p.add_argument("profile")
    p.add_argument("--skse", type=Path, required=True)
    p.add_argument("--dist", type=Path, default=ROOT / "dist")
    p.set_defaults(func=cmd_deploy)
    p = sub.add_parser("launch")
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--until-loaded", action="store_true", help="stop once SKSE and module logs are written (default)")
    mode.add_argument("--interactive", action="store_true", help="wait for the player to quit")
    p.add_argument("--timeout", type=int, default=300)
    p.add_argument("--settle", type=int, default=20)
    p.set_defaults(func=cmd_launch)
    sub.add_parser("undeploy").set_defaults(func=cmd_undeploy)
    sub.add_parser("status").set_defaults(func=cmd_status)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
