#!/usr/bin/env python3
"""Isolated SkyCRPG test profiles for a Steam/Proton Skyrim SE install.

A profile is a staging directory outside the repo and outside Steam:

    <profile-root>/<profile>/saves/   copied fixture saves (owner originals untouched)
    <profile-root>/<profile>/runs/    collected logs, one directory per launch
    <profile-root>/active.json        journal of every path the deployed profile owns

`deploy` validates every destination before writing anything: archive and SKSE
entries must be relative, free of traversal and symlinks, and land inside the
game directory (SKSE executables) or its Data/ directory; no two sources may
claim the same path. Ownership is journaled before each write, and files are
written to a temporary name and linked into place without overwriting, so an
interrupted deploy is recoverable with `undeploy`. `undeploy` removes only
paths whose content still matches what the tool wrote; anything changed since
(including an edited SkyrimCustom.ini) is kept and reported, and the profile
stays marked for reconciliation. Only one profile can be deployed at a time,
and `launch` refuses to start while any SkyrimSE.exe is running.

    tools/profile.py init <profile> [--save NAME ...]
    tools/profile.py deploy <profile> --skse <extracted skse64_2_03_01 dir> [--dist dist/]
    tools/profile.py launch [--until-loaded | --interactive] [--timeout SECONDS]
    tools/profile.py undeploy [--release-kept]
    tools/profile.py status

Exit status: 0 success; 1 error or failed/unsuccessful launch; 2 processes left
running after a launch; 3 undeploy kept changed files for manual reconciliation.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import uuid
import zipfile
from pathlib import Path, PurePosixPath

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
SKSE_RUNTIME_MARKER = "SKSE64 runtime: initialize (version = 2.3.1 01070680"
TEMP_SUFFIX = ".skycrpg-partial"
POLL_SECONDS = 2.0


class ProfileError(Exception):
    pass


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fail(message: str) -> None:
    raise ProfileError(message)


def profile_dir(name: str) -> Path:
    if name not in PROFILES:
        fail(f"unknown profile {name!r}; choose from {', '.join(PROFILES)}")
    return PROFILE_ROOT / name


def save_dir_relative(name: str) -> str:
    return f"SkyCRPG\\{name}\\Saves\\"


def custom_ini_content(name: str) -> bytes:
    return f"[General]\r\nSLocalSavePath={save_dir_relative(name)}\r\n".encode()


def load_active() -> dict | None:
    return json.loads(ACTIVE.read_text()) if ACTIVE.exists() else None


def save_active(manifest: dict) -> None:
    """Replace the journal atomically so an interruption never leaves it half-written."""
    ACTIVE.parent.mkdir(parents=True, exist_ok=True)
    tmp = ACTIVE.with_name(ACTIVE.name + ".tmp")
    with open(tmp, "w") as f:
        json.dump(manifest, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, ACTIVE)


def skyrim_pids() -> list[int]:
    """PIDs whose argv[0] is SkyrimSE.exe (Wine shows a Windows path), not any command mentioning it."""
    pids = []
    for proc in Path("/proc").iterdir():
        if proc.name.isdigit():
            try:
                argv0 = (proc / "cmdline").read_bytes().split(b"\0")[0].decode(errors="replace")
            except OSError:
                continue
            if argv0.replace("\\", "/").rsplit("/", 1)[-1].lower() == "skyrimse.exe":
                pids.append(int(proc.name))
    return pids


def cmd_init(args) -> int:
    pdir = profile_dir(args.profile)
    (pdir / "saves").mkdir(parents=True, exist_ok=True)
    (pdir / "runs").mkdir(exist_ok=True)
    copied = []
    for name in args.save:
        src = OWNER_SAVES / name
        if PurePosixPath(name).name != name or not src.is_file():
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
    return 0


def safe_destination(base: Path, relative: str, source: str) -> Path:
    """Resolve `relative` under `base`, refusing absolute, traversal, drive or empty components."""
    if not relative or "\\" in relative or "\0" in relative:
        fail(f"{source}: invalid path {relative!r}")
    rel = PurePosixPath(relative)
    if rel.is_absolute() or re.match(r"^[A-Za-z]:", relative):
        fail(f"{source}: absolute path {relative!r} refused")
    if any(part in ("", ".", "..") for part in relative.split("/")):
        fail(f"{source}: path {relative!r} contains traversal or empty components")
    dest = base.joinpath(*rel.parts)
    real_base = os.path.realpath(base)
    if os.path.commonpath([real_base, os.path.realpath(dest)]) != real_base:
        fail(f"{source}: {relative!r} resolves outside {base}")
    return dest


def plan_files(profile: str, skse_dir: Path, dist: Path) -> dict[Path, bytes]:
    """Map destination path -> content for the profile's prerequisites and packages, fully validated."""
    files: dict[Path, bytes] = {}
    claimed: dict[str, str] = {}

    def claim(dest: Path, data: bytes, source: str) -> None:
        key = str(dest).lower()  # the game sees a case-insensitive filesystem through Wine
        if key in claimed:
            fail(f"{dest} is claimed by both {claimed[key]} and {source}")
        claimed[key] = source
        files[dest] = data

    if not (skse_dir / "skse64_loader.exe").is_file():
        fail(f"{skse_dir / 'skse64_loader.exe'} missing; pass the extracted SKSE64 2.3.1 directory")
    for src in sorted(skse_dir.rglob("*")):
        rel = src.relative_to(skse_dir)
        if src.is_symlink():
            fail(f"SKSE: symlink {rel} refused")
        if not src.is_file():
            continue
        if rel.parts[0] == "Data" and len(rel.parts) > 1:
            dest = safe_destination(GAME / "Data", PurePosixPath(*rel.parts[1:]).as_posix(), "SKSE")
        elif len(rel.parts) == 1 and src.suffix in (".exe", ".dll"):
            dest = safe_destination(GAME, rel.as_posix(), "SKSE")
        else:
            continue  # readme, source tarball
        claim(dest, src.read_bytes(), "SKSE")

    for package in PROFILES[profile]["packages"]:
        archives = sorted(dist.glob(f"{package}-*.zip"))
        if len(archives) != 1:
            fail(f"expected exactly one {package}-*.zip in {dist}, found {len(archives)}")
        with zipfile.ZipFile(archives[0]) as z:
            for info in z.infolist():
                if info.is_dir():
                    continue
                if (info.external_attr >> 16) & 0o170000 == 0o120000:
                    fail(f"{archives[0].name}: symlink entry {info.filename!r} refused")
                dest = safe_destination(GAME / "Data", info.filename, archives[0].name)
                claim(dest, z.read(info), archives[0].name)
    return files


def install_file(dest: Path, data: bytes) -> None:
    """Write via a temporary sibling and hard-link into place; never replaces an existing file."""
    tmp = dest.with_name(dest.name + TEMP_SUFFIX)
    with open(tmp, "xb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    try:
        os.link(tmp, dest)
    finally:
        tmp.unlink()


def cmd_deploy(args) -> int:
    if load_active():
        fail(f"profile {load_active()['profile']!r} is deployed or needs reconciliation; run undeploy first")
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

    # Validate everything before the first write.
    files = plan_files(args.profile, args.skse.resolve(), args.dist.resolve())
    for dest, data in files.items():
        if dest.exists() and (dest.is_symlink() or not dest.is_file() or dest.read_bytes() != data):
            fail(f"{dest} exists with different content; refusing to overwrite")
        if dest.with_name(dest.name + TEMP_SUFFIX).exists():
            fail(f"stale partial file next to {dest}; inspect it first")
    ini_data = custom_ini_content(args.profile)

    manifest = {"profile": args.profile, "state": "deploying", "deployed_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                "exe_sha256": exe_hash, "files": [], "dirs": [], "kept": []}
    save_active(manifest)
    targets = sorted(files.items()) + [(CUSTOM_INI, ini_data)]
    for dest, data in targets:
        for parent in reversed(dest.parents):
            if not parent.exists():
                manifest["dirs"].append(str(parent))  # journaled before creation
                save_active(manifest)
                parent.mkdir()
        entry = {"path": str(dest), "sha256": hashlib.sha256(data).hexdigest(), "preexisting": dest.exists()}
        manifest["files"].append(entry)  # journaled before the write
        save_active(manifest)
        if not entry["preexisting"]:
            install_file(dest, data)

    saves = MY_GAMES / "SkyCRPG" / args.profile / "Saves"
    saves.mkdir(parents=True, exist_ok=True)
    for ess in (pdir / "saves").iterdir():
        if not (saves / ess.name).exists():
            shutil.copy2(ess, saves / ess.name)
    manifest["state"] = "deployed"
    save_active(manifest)
    print(f"deployed {args.profile}: {len(files)} files, saves in {saves}")
    return 0


def cmd_undeploy(args) -> int:
    manifest = load_active()
    if not manifest:
        print("nothing deployed")
        return 0
    if skyrim_pids():
        fail("SkyrimSE.exe is running")
    if args.release_kept:
        if manifest.get("state") != "needs_reconciliation":
            fail("--release-kept applies only to a profile marked needs_reconciliation")
        ACTIVE.unlink()
        print(f"released {manifest['profile']}; kept paths are now unmanaged: {manifest['kept']}")
        return 0

    kept = []
    for entry in reversed(manifest["files"]):
        path = Path(entry["path"])
        tmp = path.with_name(path.name + TEMP_SUFFIX)
        if tmp.exists() and not entry["preexisting"]:
            tmp.unlink()  # interrupted before link; the partial file is ours by construction
        if entry["preexisting"] or not path.exists():
            continue
        if path.is_symlink() or not path.is_file() or sha256(path) != entry["sha256"]:
            kept.append(str(path))
            print(f"kept {path}: changed since deploy; reconcile manually")
            continue
        path.unlink()
    for d in reversed(manifest["dirs"]):
        try:
            Path(d).rmdir()
        except FileNotFoundError:
            pass
        except OSError:
            print(f"kept non-empty directory {d}")

    if kept:
        manifest.update(state="needs_reconciliation", kept=kept)
        save_active(manifest)
        print(f"{manifest['profile']} needs reconciliation: inspect the kept paths, then run undeploy --release-kept")
        return 3
    ACTIVE.unlink()
    print(f"undeployed {manifest['profile']}; profile saves remain in {MY_GAMES / 'SkyCRPG' / manifest['profile']}")
    return 0


def tagged_pids(tag: str) -> list[int]:
    pids = []
    for proc in Path("/proc").iterdir():
        if proc.name.isdigit():
            try:
                if f"SKYCRPG_RUN_ID={tag}".encode() in (proc / "environ").read_bytes():
                    pids.append(int(proc.name))
            except OSError:
                pass
    return [p for p in pids if p != os.getpid()]


def stop_tagged(tag: str) -> list[int]:
    """Stop only processes started by this launch, identified by their environment tag."""
    for sig in (signal.SIGTERM, signal.SIGKILL):
        for pid in tagged_pids(tag):
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                pass
        deadline = time.time() + 15
        while tagged_pids(tag) and time.time() < deadline:
            time.sleep(0.5)
    return tagged_pids(tag)


def fresh_text(name: str, started: float) -> str | None:
    path = SKSE_LOGS / name
    if path.exists() and path.stat().st_mtime >= started:
        return path.read_text(errors="replace")
    return None


def load_problems(profile: str, started: float) -> list[str]:
    """Positive markers required for LOADED; returns what is missing or rejected (empty = loaded)."""
    problems = []
    skse = fresh_text("skse64.log", started)
    if skse is None:
        return ["no fresh skse64.log"]
    if SKSE_RUNTIME_MARKER not in skse:
        problems.append("skse64.log lacks SKSE 2.3.1 initialization for runtime 1.7.104")
    if "dispatch message (8)" not in skse:
        problems.append("skse64.log has not reached data load (message 8)")
    for package in PROFILES[profile]["packages"]:
        if not re.search(rf"^plugin {re.escape(package)}\.dll \(.*\) loaded correctly", skse, re.M):
            problems.append(f"SKSE did not report {package}.dll loaded correctly")
        module = fresh_text(f"{package}.log", started)
        if module is None:
            problems.append(f"no fresh {package}.log")
        elif "runtime check: PASS" not in module:
            problems.append(f"{package}.log lacks a runtime PASS")
    return problems


def collect(run_dir: Path, started: float) -> dict:
    run_dir.mkdir(parents=True, exist_ok=True)
    found = {}
    packages = sorted({pkg for spec in PROFILES.values() for pkg in spec["packages"]})
    for name in ["skse64.log", "skse64_loader.log"] + [f"{p}.log" for p in packages]:
        path = SKSE_LOGS / name
        if path.exists() and path.stat().st_mtime >= started:
            shutil.copy2(path, run_dir / name)
            found[name] = sha256(path)
    return found


def launch_command() -> list[str]:
    return [str(RUNTIME / "_v2-entry-point"), "--verb=waitforexitandrun", "--",
            str(PROTON / "proton"), "waitforexitandrun", str(GAME / "skse64_loader.exe")]


def cmd_launch(args) -> int:
    manifest = load_active()
    if not manifest or manifest.get("state") != "deployed":
        fail("deploy a profile first")
    if skyrim_pids():
        fail("SkyrimSE.exe is already running; one Skyrim process per profile")
    profile = manifest["profile"]
    tag = uuid.uuid4().hex
    started = time.time()
    run_dir = profile_dir(profile) / "runs" / time.strftime("%Y%m%dT%H%M%S")
    run_dir.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ,
               SKYCRPG_RUN_ID=tag,
               SteamAppId="489830", SteamGameId="489830", STEAM_COMPAT_APP_ID="489830",
               STEAM_COMPAT_DATA_PATH=str(COMPAT),
               STEAM_COMPAT_CLIENT_INSTALL_PATH=str(STEAM),
               STEAM_COMPAT_INSTALL_PATH=str(GAME),
               PROTON_LOG="1", PROTON_LOG_DIR=str(run_dir))
    command = launch_command()
    mode = "interactive" if args.interactive else "until-loaded"
    with open(run_dir / "launch.log", "w") as log:
        proc = subprocess.Popen(command, cwd=GAME, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    record = {"profile": profile, "run_id": tag, "pid": proc.pid, "command": command, "mode": mode,
              "started": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "timeout_s": args.timeout}
    (run_dir / "run.json").write_text(json.dumps(record, indent=2))
    print(f"launched {profile}: pid {proc.pid}, run {run_dir}, timeout {args.timeout}s", flush=True)

    # until-loaded: LOADED once every positive marker is present while the game runs.
    # interactive: QUIT when the player exits after a verified load; anything else is a failure.
    result = "TIMEOUT"
    loaded = False
    deadline = started + args.timeout
    while time.time() < deadline:
        if not loaded and not load_problems(profile, started) and skyrim_pids():
            loaded = True
            if not args.interactive:
                time.sleep(args.settle)  # let SKSE finish writing and the game settle before stopping it
                result = "LOADED" if not load_problems(profile, started) else "LOAD_FAILED"
                break
        if proc.poll() is not None:
            result = "QUIT" if args.interactive and loaded else "EXITED"
            break
        time.sleep(POLL_SECONDS)
    if result == "TIMEOUT" and fresh_text("skse64.log", started) is not None and load_problems(profile, started):
        result = "LOAD_FAILED"

    leftover = stop_tagged(tag)
    problems = load_problems(profile, started)
    success = result == ("QUIT" if args.interactive else "LOADED")
    record.update(result=result, success=success, exit_code=proc.poll(), load_problems=problems,
                  ended=time.strftime("%Y-%m-%dT%H:%M:%S%z"), logs=collect(run_dir, started),
                  leftover_pids=leftover, skyrim_running_after=skyrim_pids())
    (run_dir / "run.json").write_text(json.dumps(record, indent=2))
    print(json.dumps({k: record[k] for k in ("result", "success", "exit_code", "load_problems", "logs",
                                             "leftover_pids", "skyrim_running_after")}, indent=2))
    if leftover:
        print(f"error: processes from this run still alive: {leftover}", file=sys.stderr)
        return 2
    return 0 if success else 1


def cmd_status(args) -> int:
    manifest = load_active()
    print(f"deployed: {manifest['profile'] + ' (' + manifest.get('state', '?') + ')' if manifest else 'none'}")
    print(f"SkyrimSE.exe running: {skyrim_pids() or 'no'}")
    for name in PROFILES:
        pdir = PROFILE_ROOT / name
        if pdir.exists():
            print(f"{name}: {len(list((pdir / 'saves').glob('*.ess')))} saves, {len(list((pdir / 'runs').iterdir()))} runs")
    return 0


def build_parser() -> argparse.ArgumentParser:
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
    mode.add_argument("--until-loaded", action="store_true", help="stop once load markers are verified (default)")
    mode.add_argument("--interactive", action="store_true", help="wait for the player to quit")
    p.add_argument("--timeout", type=int, default=300)
    p.add_argument("--settle", type=int, default=20)
    p.set_defaults(func=cmd_launch)
    p = sub.add_parser("undeploy")
    p.add_argument("--release-kept", action="store_true", help="forget kept paths after manual reconciliation")
    p.set_defaults(func=cmd_undeploy)
    sub.add_parser("status").set_defaults(func=cmd_status)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except ProfileError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
