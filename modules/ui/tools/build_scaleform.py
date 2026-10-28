#!/usr/bin/env python3
"""Build a local, dependency-requiring spike from a pinned external SkyUI checkout.

Never modifies the provider tree, deploys to Skyrim, or downloads third-party work.
External SWFs and generated derivatives stay in an explicit output directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

PIN = "3d537a366f1a45c14a7012ea0aa00be18108f073"
ROOT = Path(__file__).resolve().parents[1]
BASE_SWFS = {
    "inventorymenu.swf": "915f8e688ab6a529aefa3cf46f4ab3a5805bc4ba22fb9f50f1aa591d577edb6d",
    "skyui/inventorylists.swf": "3ac8dd993fe693386c65ccf9a33c73151004f87c2634b06c95069ba524b3d033",
    "hudmenu.swf": "b586c7e1b13b4eee83e820566027243314d5019dbefc36bcac85f247325a22db",
}
NEXUS_BASE_SWFS = {
    "inventorymenu.swf": "9225ed93e1589e69f6d5305299e8e6847aaaea1323a3c39ba88360803c3d49a6",
    "skyui/inventorylists.swf": "6a762b62dbc58d8ea871620e181b1e950abd32b0cfd726ed44d1aba350469061",
    "hudmenu.swf": "e8277c1fedcda3ac8c159441f4e7c55b62fbe564ec3cc1bd602b70427f0a47cf",
}
SOURCES = {
    "ItemMenus/InventoryMenu.as": "71142a8bfa6851b7c250f9ef255a386f800dccd59d7c080be8e798b9c0cabd1d",
    "ItemMenus/InventoryLists.as": "03fc18ae04c1fdb5b435b83e20d7414773e287ceb5fb4dd2aa0d3b07e54f68f3",
    "Common/skyui/components/list/ScrollingList.as": "af6ddbd9d66385024c614afce7deffffa1346ade09cae25838058ec544b2ba4c",
    "Common/skyui/components/list/TabularList.as": "ae576a7ac6ae29827edda04c6f70f09b896a3de6426c7083c9118c50b5f81efa",
    "Common/skyui/components/list/TabularListEntry.as": "52093027024bc0b089a7c752be6edcd3fa3767cda9f1d65a7e9f14e4432dd8e5",
    "Vanilla/HUDMenu.as": "de9d6d2a3e5bca549b2a699fa334d32396f9671ab68a81e9a185ce9ec45a5eb5",
}
# InventoryLists is defined in both the menu and its imported symbol movie.
# Patch both: the registered external symbol supplies the live panel instance.
TARGETS = {
    "inventorymenu.swf": ["ItemMenus/InventoryMenu.as", "ItemMenus/InventoryLists.as"],
    "skyui/inventorylists.swf": [n for n in SOURCES if n.startswith("Common/")] + ["ItemMenus/InventoryLists.as"],
    "hudmenu.swf": ["Vanilla/HUDMenu.as"],
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(text, anchor, replacement):
    if text.count(anchor) != 1:
        raise ValueError(f"Expected one patch anchor: {anchor!r}")
    return text.replace(anchor, replacement, 1)


def hud_init_source():
    """HUD initialization as compiled: shared init with feature panels substituted."""
    hud = (ROOT / "scaleform/hud-init.as.inc").read_text()
    return replace_once(hud, "         // SKYCRPG_FOLLOWER_PANEL", (ROOT / "scaleform/follower-panel.as.inc").read_text().rstrip())


def transform(name, text):
    if name.endswith("InventoryMenu.as"):
        text = replace_once(text, "_loc3_.layout = _loc5_;", "_loc3_.skycrpgGrid = true;\n      _loc3_.scrollDelta = 3;\n      _loc3_.layout = _loc5_;")
        # DYL-163: gate the existing player-only dispatch sites. No new native action.
        text = replace_once(text, "   function handleInput(details, pathToFocus)\n   {",
                            "   function handleInput(details, pathToFocus)\n   {\n      this.skycrpgInventoryInput(details);")
        text = replace_once(text, '         gfx.io.GameDelegate.call("ItemSelect",[a_slot]);',
                            '         if(!this.skycrpgInventoryAllow("activate",1,"dispatch")) return;\n         gfx.io.GameDelegate.call("ItemSelect",[a_slot]);')
        text = replace_once(text, '         gfx.io.GameDelegate.call("ItemSelect",[]);',
                            '         if(this.skycrpgInventoryBridge() != undefined && event.entry != this.inventoryLists.itemList.selectedEntry) return;\n         if(!this.skycrpgInventoryAllow("activate",1,"dispatch")) return;\n         gfx.io.GameDelegate.call("ItemSelect",[]);')
        drop = "      if(this.shouldProcessItemsListInput(false) && this.inventoryLists.itemList.selectedEntry != undefined)\n      {"
        text = replace_once(text, drop, drop + '\n         if(!this.skycrpgInventoryAllow("drop",1,"quantity")) return;')
        text = replace_once(text, '      gfx.io.GameDelegate.call("ItemDrop",[event.amount]);',
                            '      if(!this.skycrpgInventoryAllow("drop",event.amount,"confirm")) return;\n      gfx.io.GameDelegate.call("ItemDrop",[event.amount]);')
        text = replace_once(text, "   function startMenuFade()\n   {",
                            "   function startMenuFade()\n   {\n      this.skycrpgInventoryCancel();")
        text = replace_once(text, "      super.onItemCardSubMenuAction(event);",
                            '      if(event.opening == false && event.canceled == true) this.skycrpgInventoryCancel();\n      super.onItemCardSubMenuAction(event);')
        text = replace_once(text, "      super.onItemHighlightChange(event);",
                            "      super.onItemHighlightChange(event);\n      this.skycrpgInventoryCapture();")
        text = replace_once(text, "   function onItemHighlightChange(event)\n   {",
                            "   function onItemHighlightChange(event)\n   {\n      this.skycrpgInventoryCapture();")
        # DYL-164: player-only presentation. Install row-aligned scrolling on
        # this instance; native shared-list bodies and other contexts are unchanged.
        text = replace_once(text, "      _loc3_.skycrpgGrid = true;",
                            "      _loc3_.skycrpgGrid = true;\n      _loc3_.updateScrollPosition = _loc3_.skycrpgUpdateScrollPosition;\n      _loc3_.calculateMaxScrollPosition = _loc3_.skycrpgCalculateMaxScrollPosition;")
        text = replace_once(text, "      _loc3_.layout = _loc5_;",
                            "      _loc3_.layout = _loc5_;\n      this.skycrpgLayout();\n      this.skycrpgRequestEquipment();")
        text = replace_once(text, "      this.skycrpgInventoryCancel();",
                            "      this.skycrpgInventoryCancel();\n      this.skycrpgPresentationClose();")
        text = replace_once(text, "      super.onItemCardSubMenuAction(event);",
                            "      super.onItemCardSubMenuAction(event);\n      this._skycrpgModal = event.opening == true;\n      this.skycrpgPositionCard();")
        pos = text.rfind("}")
        text = text[:pos] + (ROOT / "scaleform/inventory-actions.as.inc").read_text() + (ROOT / "scaleform/inventory-presentation.as.inc").read_text() + text[pos:]
    elif name.endswith("InventoryLists.as"):
        # DYL-168: controller search consumes input before native sort/search.
        search_anchor = "      if(this._platform != 0)\n      {\n         if(details.skseKeycode == this._sortOrderKey)"
        text = replace_once(text, search_anchor,
                            '      if(this.skycrpgSearchInput(details)) return true;\n' + search_anchor)
        # Reflect native search ownership/caret and keep the filter caption fresh.
        for callback, tail in (
                ("onSearchInputStart(event)", '      this._nameFilter.filterText = "";'),
                ("onSearchInputEnd(event)", "      this._nameFilter.filterText = event.data;")):
            start = text.index("   function " + callback)
            end = text.index("\n   }", start)
            body = text[start:end]
            body = replace_once(body, tail, tail + "\n      this.skycrpgDrawFocus();")
            if callback.startswith("onSearchInputEnd"):
                body += "\n      if(this.itemList.skycrpgGrid) this.skycrpgSearchLabel(event.data);"
            text = text[:start] + body + text[end:]
        anchor = "if(this.categoryList.handleInput(details,pathToFocus))"
        text = replace_once(text, anchor, (ROOT / "scaleform/grid-routing.as.inc").read_text() + "\n      " + anchor)
        # Shared lists notify only the player menu; other contexts have no hook.
        for method in ("InvalidateListData()", "onFilterChange()", "onSortChange(event)", "showItemsList()"):
            view_anchor = "   function " + method + "\n   {"
            text = replace_once(text, view_anchor, view_anchor +
                                '\n      if(typeof _root.Menu_mc.skycrpgInventoryViewChanged == "function") _root.Menu_mc.skycrpgInventoryViewChanged();')
        pos = text.rfind("}")
        text = text[:pos] + (ROOT / "scaleform/inventory-list-layout.as.inc").read_text() + text[pos:]
    elif name.endswith("ScrollingList.as"):
        text = replace_once(text, "// That makes no sense, does it?", "if (this.skycrpgMove(details)) return true;\n\n        // That makes no sense, does it?")
        text = replace_once(text, "this.setClipCount(this._maxListIndex);", "if (this.skycrpgGrid) this._maxListIndex = 18;\n        this.setClipCount(this._maxListIndex);")
        text = replace_once(text, "entryClip._x = xStart;\n            entryClip._y = yStart + h;", "entryClip._x = xStart + (this.skycrpgGrid ? (this._listIndex % 3) * 172 : 0);\n            entryClip._y = yStart + (this.skycrpgGrid ? Math.floor(this._listIndex / 3) * 74 : h);")
        text = replace_once(text, "var t = this.getListEnumSize() - this._maxListIndex;", "if (this.skycrpgGrid) this._maxListIndex = 18;\n        var t = this.getListEnumSize() - this._maxListIndex;")
        pos = text.rfind("}")
        text = text[:pos] + (ROOT / "scaleform/grid-methods.as.inc").read_text() + text[pos:]
    elif name.endswith("TabularList.as"):
        text = replace_once(text, "this._maxListIndex = Math.floor((this._listHeight / this.entryHeight) + 0.05);", "this._maxListIndex = this.skycrpgGrid ? 18 : Math.floor((this._listHeight / this.entryHeight) + 0.05);")
    elif name.endswith("TabularListEntry.as"):
        # The same local declaration exists in two functions: target the method signature.
        anchor = "public function setEntry(a_entryObject: Object, a_state: ListState)\n    {"
        text = replace_once(text, anchor, anchor + "\n        if (a_state.list.skycrpgGrid) {\n            a_state.list.skycrpgPaintCell(this, a_entryObject);\n            return;\n        }")
    elif name.endswith("HUDMenu.as"):
        mode_anchor = "         _loc2_ = _loc2_ + 1;\n      }\n   }\n   function SetLocationName(aLocation)"
        mode_sync = """         _loc2_ = _loc2_ + 1;
      }
      if (this.SkyCRPGStatus != undefined) {
         var skycrpgDataReady = this.SkyCRPGStatus._skycrpgDataReady == true;
         var skycrpgVisible = skycrpgDataReady && this.CheckAgainstHudMode(this.SkyCRPGStatus);
         this.SkyCRPGStatus._visible = skycrpgVisible;
         if (typeof(this.SkyCRPGPartyArea.syncVisibility) == "function") {
            this.SkyCRPGPartyArea.syncVisibility();
         } else {
            this.SkyCRPGPartyArea._visible = this.CheckAgainstHudMode(this.SkyCRPGPartyArea);
         }
         this._skycrpgMetersSuppressed = skycrpgVisible;
         this.Health._visible = skycrpgVisible ? false : this.CheckAgainstHudMode(this.Health);
         this.Magica._visible = skycrpgVisible ? false : this.CheckAgainstHudMode(this.Magica);
         this.Stamina._visible = skycrpgVisible ? false : this.CheckAgainstHudMode(this.Stamina);
      }
   }
   function SetLocationName(aLocation)"""
        text = replace_once(text, mode_anchor, mode_sync)
        compass_write = "      this.CompassRect._parent._visible = abShowCompass;"
        # The engine calls SetCompassAngle every frame. While the SkyCRPG lease is
        # held, record the native-desired value and keep the compass hidden in the
        # same call, so it never shows for a frame between lease samples.
        text = replace_once(text, compass_write, compass_write + "\n      if (this._skycrpgCompassOwned == true) {\n         this._skycrpgCompassPrior = abShowCompass;\n         this.CompassRect._parent._visible = false;\n      }")
        anchor = 'Stage.scaleMode = "showAll";'
        text = replace_once(text, anchor, anchor + "\n" + hud_init_source())
        text = replace_once(text, anchor, anchor + "\n" + (ROOT / "scaleform/compass-policy.as.inc").read_text())
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skyui", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--nexus-interface", type=Path,
                        help="Extracted Interface folder from acquired Nexus SkyUI6.11 BSA; exact hashes checked")
    parser.add_argument("--java", type=Path)
    parser.add_argument("--target", choices=sorted(TARGETS), action="append",
                        help="Compile only this changed SWF; repeat to select multiple targets (default: all)")
    parser.add_argument("--owner", default="DYL-153", help="Evidence owner recorded in build.json")
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    provider = args.skyui.resolve()
    out = args.out.resolve()
    # Prevent accidental provider edits or game deployment through this builder.
    if out == provider or provider in out.parents or out.exists():
        parser.error("--out must be a new directory outside the provider checkout")
    commit = subprocess.check_output(["git", "-C", str(provider), "rev-parse", "HEAD"], text=True).strip()
    if commit != PIN:
        parser.error(f"Expected SkyUI {PIN}, got {commit}")
    texts = {}
    for name, expected in SOURCES.items():
        source = provider / "source/actionscript" / name
        if sha(source) != expected:
            parser.error(f"Provider source drift: {name}")
        texts[name] = transform(name, source.read_text())
    base = args.nexus_interface.resolve() if args.nexus_interface else provider / "data/interface"
    base_hashes = NEXUS_BASE_SWFS if args.nexus_interface else BASE_SWFS
    for name, expected in base_hashes.items():
        if sha(base / name) != expected:
            parser.error(f"Provider SWF drift: {name}")
    manifest = {"skyui_commit": commit, "status": "PREPARED", "source_sha256": SOURCES,
                "base": "Nexus6.11 acquired BSA" if args.nexus_interface else "GitHub source-tree SWFs (compile probe)",
                "patch_sha256": {p.name: sha(p) for p in (ROOT / "scaleform").glob("*.inc")},
                "ffdec_cli_sha256": sha(provider / "tools/FFDec/ffdec-cli.jar"),
                "targets": args.target or list(TARGETS),
                "owner": args.owner,
                "inputs": {}, "outputs": {}, "jobs": []}
    out.mkdir(parents=True)
    for target in manifest["targets"]:
        names = TARGETS[target]
        stage = out / "staging" / target
        for name in names:
            file = stage / "__Packages" / name.split("/", 1)[1]
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text(texts[name])
        source_swf = base / target
        manifest["inputs"][target] = sha(source_swf)
        if args.prepare_only:
            continue
        if not args.java:
            parser.error("--java is required unless --prepare-only")
        output = out / "Data/Interface" / target
        output.parent.mkdir(parents=True, exist_ok=True)
        command = [str(args.java), "-Xmx512m", "-Djava.awt.headless=true", "-jar", str(provider / "tools/FFDec/ffdec-cli.jar"),
                   "-config", "autoDeobfuscate=false,decompile=false", "-onerror", "abort",
                   "-importScript", str(source_swf), str(output), str(stage)]
        log = out / (target.replace("/", "-") + ".log")
        job = {"purpose": "Compile original grid/HUD patch into external SkyUI SWF", "command": command,
               "timeout_seconds": 600, "owner": args.owner, "log": str(log),
               "completion": "zero exit and nonempty SWF", "cleanup": "kill compiler process on timeout; retain logs"}
        manifest["jobs"].append(job)
        (out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
        with log.open("w") as stream:
            started = time.monotonic()
            process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT)
            job["pid"] = process.pid
            (out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
            try:
                code = process.wait(timeout=600)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                job["result"] = "TIMEOUT"
                manifest["status"] = "FAIL"
                (out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
                raise
        job["elapsed_seconds"] = round(time.monotonic() - started, 3)
        job["exit_code"] = code
        if code or not output.is_file() or output.stat().st_size == 0:
            manifest["status"] = "FAIL"
            (out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
            raise RuntimeError(f"FFDec failed for {target}; see {log}")
        manifest["outputs"][target] = sha(output)
    manifest["status"] = "PREPARED" if args.prepare_only else "COMPILED_NOT_RUN"
    (out / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
