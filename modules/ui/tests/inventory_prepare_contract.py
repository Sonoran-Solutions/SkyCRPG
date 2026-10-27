"""DYL-163 builder contract: pinned InventoryMenu/InventoryLists transforms.

Run under unittest discovery, or directly with an output directory to also
write the transformed InventoryMenu.as for inventory_actions_contract.js:
  SKYCRPG_SKYUI_SOURCE=<skyui-6.11> python3 -I modules/ui/tests/inventory_prepare_contract.py <out-dir>
"""
import importlib.util
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load_builder():
    spec = importlib.util.spec_from_file_location('inventory_builder', ROOT / 'tools/build_scaleform.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    return builder


def check(out):
    builder = load_builder()
    provider = Path(os.environ['SKYCRPG_SKYUI_SOURCE']) / 'source/actionscript'
    for name, expected in builder.SOURCES.items():
        source = provider / name
        assert builder.sha(source) == expected, name
        original = source.read_text()
        transformed = builder.transform(name, original)
        if name.endswith('InventoryMenu.as'):
            # Gate existing native calls; never add or remove a mutation callback.
            pattern = r'gfx\.io\.GameDelegate\.[^;]+;'
            assert re.findall(pattern, original) == re.findall(pattern, transformed)
            assert transformed.count('skycrpgInventoryAllow("activate"') == 2
            assert transformed.count('skycrpgInventoryAllow("drop"') == 2
            highlight = re.search(r'function onItemHighlightChange\(event\)\s*\{.*?\n   \}', transformed, re.S).group()
            assert highlight.count('this.skycrpgInventoryCapture();') == 2
            assert highlight.index('this.skycrpgInventoryCapture();') < highlight.index('super.onItemHighlightChange(event);')
            (out / 'InventoryMenu.as').write_text(transformed)
        if name.endswith('InventoryLists.as'):
            assert transformed.count('_root.Menu_mc.skycrpgInventoryViewChanged();') == 4
            invalidation = re.search(r'function InvalidateListData\(\)\s*\{.*?\n   \}', transformed, re.S).group()
            assert invalidation.index('_root.Menu_mc.skycrpgInventoryViewChanged();') < invalidation.index('this.itemList.InvalidateData();')
            (out / 'InventoryLists.as').write_text(transformed)


@unittest.skipUnless(os.environ.get('SKYCRPG_SKYUI_SOURCE'), 'SKYCRPG_SKYUI_SOURCE not set')
class InventoryPrepareContract(unittest.TestCase):
    def test_native_rebuild_sampling_guard(self):
        # Portable callbacks cannot execute the engine adapter on this host.
        # Pin its safety ordering as well: neither dirty-list inspection nor
        # stale-token handling may run before the in-progress refusal.
        bridge = (ROOT / 'src/inventory/inventory_bridge.cpp').read_text()
        sample = bridge[bridge.index('bool Sample('):bridge.index('void Invalidate(')]
        self.assertRegex(sample, r'bool Sample\([^)]*\)\s*\{\s*if \(g_reconciliation\.Updating\(\)\) return false;')
        self.assertRegex(bridge, r'\{\s*const ListReconciliation::UpdateScope updating\(g_reconciliation\);\s*'
                         r'g_dirty.store\(true\);[^\n]*\n\s*menu->GetRuntimeData\(\).itemList->Update\(player\);\s*\}\s*'
                         r'g_dirty.store\(false\);')
        guard = bridge.index('if (_operation == Operation::Validate && g_reconciliation.Updating())')
        snapshot = bridge.index('if (_operation == Operation::Snapshot)')
        arguments = bridge.index('if (params.argCount != 4')
        self.assertLess(guard, snapshot)
        self.assertLess(guard, arguments)
        self.assertRegex(bridge[guard:snapshot], r'SetBoolean\(false\);\s*return;')

    def test_transforms(self):
        with tempfile.TemporaryDirectory() as out:
            check(Path(out))


if __name__ == '__main__':
    check(Path(sys.argv[1]))
    print('PASS: all pinned transforms, native dispatch preservation, player-only action gates and view invalidation anchors')
