"""Ordered location-type hypotheses from keywords, names and kit prefixes."""
import re

KEYWORDS = [
    ('dwemer_ruin', ('LocSetDwarvenRuin', 'LocTypeDwarvenAutomatons')),
    ('nordic_ruin', ('LocSetNordicRuin', 'LocTypeDraugrCrypt')),
    ('cave_or_mine', ('LocSetCave', 'LocSetCaveIce', 'LocTypeMine', 'LocTypeFalmerHive')),
    ('fort_keep_prison', ('LocSetMilitaryFort', 'LocTypeMilitaryFort', 'LocTypeJail')),
    ('great_hall_palace', ('LocTypeCastle',)),
    ('multi_storey_house_inn', ('LocTypeInn',)),
    ('small_single_storey', ('LocTypeHouse', 'LocTypeDwelling', 'LocTypePlayerHouse', 'LocTypeStore', 'LocTypeTemple', 'LocTypeGuild')),
]
TOKENS = [
    ('interior_worldspace', ('blackreach', 'soulcairn', 'apocrypha', 'forgottenvale')),
    ('dwemer_ruin', ('dwemer', 'dwarven', 'alftand', 'mzulft', 'nchuand')),
    ('nordic_ruin', ('nordic', 'draugr', 'barrow', 'crypt')),
    ('cave_or_mine', ('cave', 'mine', 'grotto')),
    ('sewer_or_tunnel', ('sewer', 'ratway', 'tunnel')),
    ('tower', ('tower', 'lighthouse')),
    ('fort_keep_prison', ('fort', 'keep', 'prison', 'jail')),
    ('great_hall_palace', ('palace', 'dragonsreach', 'greathall')),
    ('ship_shack_tent_cellar', ('ship', 'shack', 'tent', 'cellar', 'basement')),
    ('multi_storey_house_inn', ('inn', 'tavern')),
    ('small_single_storey', ('dwelling', 'farmhouse')),
]
KITS = [('dwemer_ruin', ('dw', 'dwr')), ('nordic_ruin', ('nor',)),
        ('cave_or_mine', ('cave', 'mine')), ('fort_keep_prison', ('fort',))]
RULES = [('keyword', n, list(v)) for n, v in KEYWORDS] + [('editor_token', n, list(v)) for n, v in TOKENS] + [('static_kit', n, list(v)) for n, v in KITS]


def words(value):
    value = re.sub(r'([a-z])([A-Z])', r'\1 \2', value or '')
    return set(re.findall(r'[a-z]+', value.lower()))


def classify_detail(cell):
    keywords = {x.lower() for x in cell.get('location_keywords', [])}
    for index, (source, name, values) in enumerate(RULES):
        if source == 'keyword':
            hit = bool(keywords.intersection(v.lower() for v in values))
        elif source == 'editor_token':
            hit = bool(words(cell.get('editor_id')).intersection(values))
        else:
            hit = any(any(base[:len(v)].lower() == v and (len(base) == len(v) or base[len(v)].isupper() or base[len(v)].isdigit() or base[len(v)] == '_') for v in values) for base in cell.get('static_bases', {}))
        if hit:
            return name, index
    return 'unclassified', None


def classify(cell):
    return classify_detail(cell)[0]


def utility_reason(cell):
    found = words(cell.get('editor_id')).intersection(('test', 'warehouse', 'holding', 'utility', 'marker', 'markers', 'voicecell', 'zcell', 'qasmoke', 'unused'))
    name = (cell.get('editor_id') or '').lower()
    found.update(prefix for prefix in ('voicecell', 'zcell') if name.startswith(prefix))
    return 'editor ID utility token: ' + ', '.join(sorted(found)) if found else None
