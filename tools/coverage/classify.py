"""Conservative, ordered Phase 5.5 location-type hypotheses."""
import re

RULES = [
    ('interior_worldspace', r'blackreach|soulcairn|apocrypha|forgottenvale'),
    ('dwemer_ruin', r'dwemer|dwarven|alftand|mzulft|nchuand'),
    ('nordic_ruin', r'nordic|draugr|barrow|crypt'),
    ('cave_or_mine', r'cave|mine|grotto'),
    ('sewer_or_tunnel', r'sewer|ratway|tunnel'),
    ('tower', r'tower|lighthouse'),
    ('fort_keep_prison', r'fort|keep|prison|jail'),
    ('great_hall_palace', r'palace|dragon(s)?reach|greathall'),
    ('ship_shack_tent_cellar', r'ship|shack|tent|cellar|basement'),
    ('multi_storey_house_inn', r'inn|tavern'),
    ('small_single_storey', r'dwelling|farmhouse'),
]


def classify(cell):
    parts = [cell.get('editor_id') or '', cell.get('lighting_template_editor_id') or '',
             cell.get('location_editor_id') or '']
    parts += cell.get('location_keywords', []) + list(cell.get('static_bases', {}))
    text = ' '.join(parts).lower()
    for name, pattern in RULES:
        if re.search(pattern, text):
            return name
    return 'unclassified'
