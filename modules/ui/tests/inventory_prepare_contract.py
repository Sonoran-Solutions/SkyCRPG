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
            (out / 'InventoryMenu.as').write_text(transformed)
        if name.endswith('InventoryLists.as'):
            assert transformed.count('_root.Menu_mc.skycrpgInventoryViewChanged();') == 4


@unittest.skipUnless(os.environ.get('SKYCRPG_SKYUI_SOURCE'), 'SKYCRPG_SKYUI_SOURCE not set')
class InventoryPrepareContract(unittest.TestCase):
    def test_transforms(self):
        with tempfile.TemporaryDirectory() as out:
            check(Path(out))


if __name__ == '__main__':
    check(Path(sys.argv[1]))
    print('PASS: all pinned transforms, native dispatch preservation, player-only action gates and view invalidation anchors')
