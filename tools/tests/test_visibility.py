"""Rendered-visibility tool (tools/visibility): PNG reader, geometry, verdict rules on synthetic frames.

No game, no third-party packages. Wave 1 frames and the frames of the first in-game oracle run
(p55-oracle-ext-1) are read only if they exist on this host (skipped otherwise).
"""
import json
from pathlib import Path
import random
import struct
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'visibility'))
import png  # noqa: E402
import rendered_visibility as rv  # noqa: E402

CLIENT = (200, 120)
PARAMS = dict(rv.PARAMS, client=CLIENT)
BORDER, TITLE = 3, 29
WAVE1 = Path('/srv/storage/Projects/SkyCRPG/session-p4/autotest/runs')
WAVE1_ROUTES = Path('/srv/storage/Projects/SkyCRPG/session-p55/routes/runs')
ORACLE_EXT_1 = Path('/srv/storage/Projects/SkyCRPG/session-p55/oracle/runs/p55-oracle-ext-1/collected')
ORACLE_EXT_1_SHOTS = WAVE1 / 'batch-p55-oracle-ext-1-00/screenshots'


# ---- PNG fixtures ------------------------------------------------------------------------------
def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)


def filtered_png(width, height, pixels, bpp, colour, filters, interlace=0):
    """Encode raw pixel bytes with the given per-row filter types (the reader must undo each)."""
    stride = width * bpp
    rows, prev = [], bytes(stride)
    for y in range(height):
        line = pixels[y * stride:(y + 1) * stride]
        f = filters[y % len(filters)]
        out = bytearray()
        for i in range(stride):
            left = line[i - bpp] if i >= bpp else 0
            up = prev[i]
            upleft = prev[i - bpp] if i >= bpp else 0
            pred = {0: 0, 1: left, 2: up, 3: (left + up) >> 1, 4: png._paeth(left, up, upleft)}[f]
            out.append((line[i] - pred) & 0xFF)
        rows.append(bytes([f]) + bytes(out))
        prev = line
    header = struct.pack('>IIBBBBB', width, height, 8, colour, 0, 0, interlace)
    return png.SIGNATURE + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b'')


class PngTests(unittest.TestCase):
    def test_round_trip_rgb(self):
        rng = random.Random(1)
        image = png.Image(7, 5, bytes(rng.randrange(256) for _ in range(7 * 5 * 3)))
        self.assertEqual(png.decode(png.encode(image)), image)

    def test_every_filter_type_rgb_and_rgba(self):
        rng = random.Random(2)
        rgb = bytes(rng.randrange(256) for _ in range(9 * 6 * 3))
        for f in range(5):
            self.assertEqual(png.decode(filtered_png(9, 6, rgb, 3, 2, [f])).rgb, rgb, f)
        self.assertEqual(png.decode(filtered_png(9, 6, rgb, 3, 2, [0, 1, 2, 3, 4])).rgb, rgb)
        rgba = bytes(rng.randrange(256) for _ in range(9 * 6 * 4))
        expect = bytes(b for i, b in enumerate(rgba) if i % 4 != 3)
        self.assertEqual(png.decode(filtered_png(9, 6, rgba, 4, 6, [4, 3, 2, 1])).rgb, expect)
        grey = bytes(rng.randrange(256) for _ in range(9 * 6))
        self.assertEqual(png.decode(filtered_png(9, 6, grey, 1, 0, [1, 4])).rgb, bytes(v for v in grey for _ in range(3)))

    def test_refusals(self):
        good = filtered_png(3, 2, bytes(18), 3, 2, [0])
        with self.assertRaises(png.PngError):
            png.decode(b'notapng' + good)
        broken = bytearray(good)
        broken[-20] ^= 0xFF  # inside the IDAT body: CRC mismatch
        with self.assertRaises(png.PngError):
            png.decode(bytes(broken))
        with self.assertRaises(png.PngError):
            png.decode(filtered_png(3, 2, bytes(18), 3, 2, [0], interlace=1))
        with self.assertRaises(png.PngError):
            png.decode(good[:-12])  # no IEND
        palette = png.SIGNATURE + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 3, 0, 0, 0)) + chunk(b'PLTE', b'\0\0\0')
        with self.assertRaises(png.PngError):
            png.decode(palette + chunk(b'IEND', b''))
        sixteen = png.SIGNATURE + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 16, 2, 0, 0, 0)) + chunk(b'IEND', b'')
        with self.assertRaises(png.PngError):
            png.decode(sixteen)

    def test_crop(self):
        image = png.Image(4, 3, bytes(range(36)))
        self.assertEqual(image.crop(1, 1, 3, 3).rgb, bytes(range(15, 21)) + bytes(range(27, 33)))
        self.assertEqual(image.crop(5, 5, 9, 9).width, 0)


# ---- synthetic scenes ----------------------------------------------------------------------------
class Scene:
    """A decorated window (3 px border, 29 px top) around a 200x120 client with a textured background."""

    def __init__(self, seed=3, dark=False):
        rng = random.Random(seed)
        self.w, self.h = CLIENT[0] + 2 * BORDER, CLIENT[1] + TITLE + BORDER
        scale = 0.12 if dark else 1.0
        self.bg = [[tuple(int((60 + rng.randrange(120)) * scale) for _ in range(3)) for _ in range(self.w)] for _ in range(self.h)]
        for y in range(self.h):
            for x in range(self.w):
                if y < TITLE or x < BORDER or x >= self.w - BORDER or y >= self.h - BORDER:
                    self.bg[y][x] = (50, 150, 250)
        self.player_colour = tuple(int(c * scale) for c in (220, 40, 40))
        self.idle_colour = tuple(int(c * scale) for c in (40, 200, 40))
        self.scale = scale
        # Player figure in client pixels: a 16 px wide body from y 28 to 86 (head point 40, feet point 80), and
        # its shadow on the ground to the lower right (B loses both, as in p55-oracle-ext-1).
        self.figure = (92, 28, 108, 86)
        self.shadow = (108, 76, 118, 86)

    def frame(self, player=True, occluders=(), shift=0, pose=0):
        """occluders: (rect in client px, colour, alpha) drawn over everything, in order. pose 1 is the same
        player one idle-animation step later: half its pixels change colour and it is 1 px wider, so A-A2
        differs over most of the figure while the silhouette stays (what the first in-game run showed)."""
        rows = []
        for y in range(self.h):
            row = bytearray()
            for x in range(self.w):
                px = self.bg[y][(x + shift) % self.w] if TITLE <= y < self.h - BORDER and BORDER <= x < self.w - BORDER else self.bg[y][x]
                cx, cy = x - BORDER, y - TITLE
                fx1 = self.figure[2] + (1 if pose else 0)
                if player and self.figure[0] <= cx < fx1 and self.figure[1] <= cy < self.figure[3]:
                    px = self.idle_colour if pose and (cx + cy) % 2 == 0 else self.player_colour
                elif player and self.shadow[0] <= cx < self.shadow[2] and self.shadow[1] <= cy < self.shadow[3]:
                    px = tuple(c // 2 for c in px)
                for (x0, y0, x1, y1), colour, alpha in occluders:
                    if x0 <= cx < x1 and y0 <= cy < y1:
                        px = tuple(int(round(alpha * c + (1 - alpha) * p)) for c, p in zip(colour, px))
                row += bytes(px)
            rows.append(bytes(row))
        return png.Image(self.w, self.h, b''.join(rows))


def view(points=None, tag='t', camera=None, available=True):
    pts = points or {'head': (100, 40), 'chest': (100, 55), 'feet': (100, 80), 'shoulder_l': (94, 48), 'shoulder_r': (106, 48)}
    entries = []
    for name in rv.POINTS:
        p = pts.get(name)
        if p is None:
            entries.append({'name': name, 'world': None, 'screen': None, 'on_screen': None})
        elif p == 'off':
            entries.append({'name': name, 'world': {}, 'screen': None, 'on_screen': False})
        else:
            entries.append({'name': name, 'world': {}, 'screen': {'x': p[0] / CLIENT[0], 'y': p[1] / CLIENT[1]},
                            'on_screen': True, 'ray': {'status': 'blocked'}})
    return {'ev': 'view_sample', 'tag': tag, 'available': available, 'reason': None if available else 'game_paused',
            'camera': camera or {'pos': {'x': 0, 'y': 0, 'z': 0}, 'yaw_deg': 45.0, 'pitch_deg': 55.0}, 'points': entries}


def triple(scene, occluders=(), restored=True, shift_b=0, idle=True):
    """A, B, A2. By default A2 shows the player one idle-animation step on (pose 1), as real frames do."""
    a = scene.frame(True, occluders)
    a2 = scene.frame(True, occluders, pose=1 if idle else 0)
    return [a, scene.frame(False, occluders, shift=shift_b), a2 if restored else scene.frame(False, occluders)]


def calibration(scene=None):
    scene = scene or Scene()
    cal = rv.calibrate(triple(scene), view(), view(), PARAMS)
    assert cal['confirmed'], cal['reason']
    return cal


class GeometryTests(unittest.TestCase):
    def test_client_origin_of_the_runner_capture(self):
        self.assertEqual(rv.client_origin(1926, 1112, 1920, 1080), (3, 29))
        self.assertEqual(rv.client_origin(206, 152, 200, 120), (3, 29))
        for bad in ((1919, 1080), (1927, 1112), (1926, 1000)):
            with self.assertRaises(rv.Indeterminate):
                rv.client_origin(bad[0], bad[1], 1920, 1080)

    def test_pixel_mapping_and_box(self):
        g = rv.Geometry((3, 29), CLIENT)
        self.assertEqual(g.pixel(0.5, 0.5), (103.0, 89.0))
        lay = rv.layout(view(), g, PARAMS)
        self.assertEqual(lay.height, 40)
        x0, y0, x1, y1 = lay.box
        self.assertLessEqual(x0, 3 + 94 - 14)
        self.assertLessEqual(y0, 29 + 40 - 14)
        self.assertGreaterEqual(y1, 29 + 80 + 14)
        self.assertTrue(lay.ring[0] <= x0 and lay.ring[2] >= x1)

    def test_layout_refusals(self):
        g = rv.Geometry((3, 29), CLIENT)
        with self.assertRaisesRegex(rv.Indeterminate, 'view_unavailable'):
            rv.layout(view(available=False), g, PARAMS)
        with self.assertRaisesRegex(rv.Indeterminate, 'box_off_screen'):
            rv.layout(view({'head': 'off', 'feet': (100, 80)}), g, PARAMS)
        with self.assertRaisesRegex(rv.Indeterminate, 'box_off_screen'):
            rv.layout(view({'head': (195, 60), 'feet': (195, 110)}), g, PARAMS)
        with self.assertRaisesRegex(rv.Indeterminate, 'box_too_small'):
            rv.layout(view({'head': (100, 70), 'feet': (100, 75)}), g, PARAMS)

    def test_dark_threshold_scales_with_luminance(self):
        self.assertEqual(rv.change_threshold(100, PARAMS), 12)
        self.assertEqual(rv.change_threshold(14, PARAMS), 6)
        self.assertEqual(rv.change_threshold(1, PARAMS), 3)


class VerdictTests(unittest.TestCase):
    def setUp(self):
        self.scene = Scene()
        self.cal = calibration(self.scene)

    def measure(self, frames, v=None, after=None, cal=None):
        return rv.measure(frames, v or view(), cal or self.cal, after if after is not None else view(), PARAMS)

    def test_calibration_confirms_the_marker(self):
        self.assertGreater(self.cal['box_signal'], 0.2)
        self.assertEqual(len(self.cal['confirmed_points']), 5)

    def test_unobstructed_player_is_visible(self):
        r = self.measure(triple(self.scene))
        self.assertEqual(r['verdict'], 'visible')
        self.assertTrue(r['restored'])
        self.assertEqual({p['verdict'] for p in r['points'].values()}, {'visible'})

    def test_opaque_occluder_is_not_visible(self):
        r = self.measure(triple(self.scene, [((80, 20, 120, 95), (90, 90, 90), 1.0)]))
        self.assertEqual(r['verdict'], 'not_visible')
        self.assertEqual({p['verdict'] for p in r['points'].values()}, {'not_visible'})

    def test_legs_occluded_is_visible_by_four_of_five_with_per_point_answers(self):
        r = self.measure(triple(self.scene, [((80, 60, 120, 95), (90, 90, 90), 1.0)]))
        self.assertEqual(r['verdict'], 'visible')  # acceptance: 4 of 5 points
        self.assertEqual(r['points']['head']['verdict'], 'visible')
        self.assertEqual(r['points']['feet']['verdict'], 'not_visible')
        self.assertEqual(r['box_verdict'], 'partly')  # the box ratio is a diagnostic beside the points

    def test_lower_body_occluded_is_partly(self):
        r = self.measure(triple(self.scene, [((80, 52, 120, 95), (90, 90, 90), 1.0)]))
        self.assertEqual(r['verdict'], 'partly')
        self.assertEqual([n for n, p in r['points'].items() if p['verdict'] == 'visible'], ['head', 'shoulder_l', 'shoulder_r'])
        self.assertEqual(r['points']['chest']['verdict'], 'not_visible')

    def test_idle_animation_between_a_and_a2_is_not_noise(self):
        """p55-oracle-ext-1: the restored player stands in the same silhouette but has moved (A-A2 0.25-0.37 of
        the box). The version 1 rule took A-A2 as the noise floor and refused the calibration."""
        r = self.measure(triple(self.scene))
        self.assertGreater(r['stats']['player_change'], 0.15)  # half the figure: about 0.18 of the box
        self.assertEqual(r['verdict'], 'visible')
        self.assertLess(r['stats']['noise'], 0.01)
        still = self.measure(triple(self.scene, idle=False))
        self.assertEqual(still['verdict'], 'visible')
        self.assertEqual(still['stats']['player_change'], 0.0)

    def test_mask_needs_b_to_differ_from_both_a_and_a2(self):
        a, b, a2 = triple(self.scene)
        g = rv.Geometry((3, 29), CLIENT)
        lay = rv.layout(view(), g, PARAMS)
        only_a = rv.mask_stats(a, b, b, lay.box, 12)  # A2 identical to B: nothing passes the mask
        self.assertGreater(only_a.ab, 0)
        self.assertEqual(only_a.mask, 0)
        full = rv.mask_stats(a, b, a2, lay.box, 12)
        self.assertGreaterEqual(full.mask, 16 * 58)  # the whole figure and its shadow inside the box
        self.assertEqual(full.mask, min(full.ab, full.a2b))

    def test_busy_ring_is_indeterminate(self):
        """Something in the ring that is only there in B passes the mask test: no trustworthy noise floor."""
        a, b, a2 = triple(self.scene)
        lay = rv.layout(view(), rv.Geometry((3, 29), CLIENT), PARAMS)
        rgb = bytearray(b.rgb)
        x0, y0, x1, y1 = lay.ring
        for y in range(y0, y1):
            for x in range(x0, x0 + 12):
                i = (y * b.width + x) * 3
                rgb[i] = (rgb[i] + 60) % 256
        r = self.measure([a, png.Image(b.width, b.height, bytes(rgb)), a2])
        self.assertEqual(r['verdict'], 'indeterminate')
        self.assertIn('scene_busy', r['reason'])

    def test_player_verdict_from_points(self):
        def pts(*verdicts):
            return {n: {'verdict': v} for n, v in zip(rv.POINTS, verdicts)}
        self.assertEqual(rv.player_verdict(pts('visible', 'visible', 'visible', 'visible', 'not_visible'), PARAMS)[0], 'visible')
        self.assertEqual(rv.player_verdict(pts('visible', 'visible', 'visible', 'off_screen', 'partly'), PARAMS)[0], 'partly')
        self.assertEqual(rv.player_verdict(pts('not_visible', 'off_screen', 'not_visible', 'not_visible', 'indeterminate'), PARAMS)[0], 'not_visible')
        verdict, why = rv.player_verdict(pts('visible', 'visible', 'visible', 'indeterminate', 'indeterminate'), PARAMS)
        self.assertEqual(verdict, 'indeterminate')
        self.assertIn('3 of 5', why)

    def test_single_fade_shows_and_eight_stacked_fades_do_not(self):
        fade = ((80, 20, 120, 95), (110, 110, 110), 0.35)
        self.assertEqual(self.measure(triple(self.scene, [fade]))['verdict'], 'visible')
        r = self.measure(triple(self.scene, [fade] * 8))  # 0.65^8 of the player difference survives
        self.assertEqual(r['verdict'], 'not_visible')

    def test_wind_noise_like_wave1_exteriors_is_tolerated(self):
        def jitter(image, share, seed):
            rng = random.Random(seed)
            rgb = bytearray(image.rgb)
            for y in range(TITLE, image.height - BORDER):
                for x in range(BORDER, image.width - BORDER):
                    if rng.random() < share:
                        i = (y * image.width + x) * 3
                        rgb[i] = (rgb[i] + 40) % 256
            return png.Image(image.width, image.height, bytes(rgb))
        a, b, a2 = triple(self.scene)
        windy = [jitter(a, 0.06, 1), jitter(b, 0.06, 2), jitter(a2, 0.06, 3)]  # wave 1 exterior pairs: 0.05-0.06
        r = self.measure(windy)
        self.assertEqual(r['verdict'], 'visible', r['reason'])
        self.assertLess(r['stats']['noise'], PARAMS['noise_max'])
        storm = [jitter(a, 0.25, 1), jitter(b, 0.25, 2), jitter(a2, 0.25, 3)]
        self.assertIn('scene_changed', self.measure(storm)['reason'])

    def test_restore_is_unprovable_for_a_hidden_player(self):
        r = self.measure(triple(self.scene, [((80, 20, 120, 95), (90, 90, 90), 1.0)]))
        self.assertIsNone(r['restored'])

    def test_scene_change_is_indeterminate(self):
        r = self.measure(triple(self.scene, shift_b=1))
        self.assertEqual(r['verdict'], 'indeterminate')
        self.assertIn('scene_changed', r['reason'])

    def test_camera_moved_between_view_samples_is_indeterminate(self):
        moved = view(camera={'pos': {'x': 30, 'y': 0, 'z': 0}, 'yaw_deg': 45.0, 'pitch_deg': 55.0})
        r = self.measure(triple(self.scene), after=moved)
        self.assertIn('camera_moved', r['reason'])
        turned = view(camera={'pos': {'x': 0, 'y': 0, 'z': 0}, 'yaw_deg': 135.0, 'pitch_deg': 55.0})
        self.assertIn('camera_moved', self.measure(triple(self.scene), after=turned)['reason'])
        unavailable = view(available=False)
        self.assertIn('view_after_unavailable', self.measure(triple(self.scene), after=unavailable)['reason'])

    def test_player_not_restored_is_reported(self):
        r = self.measure(triple(self.scene, restored=False))
        self.assertEqual(r['verdict'], 'indeterminate')
        self.assertFalse(r['restored'])
        self.assertIn('not_restored', r['reason'])

    def test_missing_or_unconfirmed_calibration_is_indeterminate(self):
        r = rv.measure(triple(self.scene), view(), None, view(), PARAMS)
        self.assertIn('calibration_unconfirmed', r['reason'])
        blind = rv.calibrate([self.scene.frame()] * 3, view(), view(), PARAMS)  # marker never toggled
        self.assertFalse(blind['confirmed'])
        self.assertIn('marker_not_confirmed', blind['reason'])
        self.assertEqual(self.measure(triple(self.scene), cal=blind)['verdict'], 'indeterminate')
        old = dict(self.cal, version=1)  # a version 1 record measured the noise differently
        self.assertIn('calibration_version', self.measure(triple(self.scene), cal=old)['reason'])

    def test_missing_frame_is_indeterminate(self):
        with tempfile.TemporaryDirectory() as tmp:
            a = Path(tmp) / 'a.png'
            png.write(a, self.scene.frame())
            with self.assertRaisesRegex(rv.Indeterminate, 'frame_missing: B'):
                rv.load_frames([a, Path(tmp) / 'b.png', a])
            small = Path(tmp) / 's.png'
            png.write(small, png.Image(2, 2, bytes(12)))
            with self.assertRaisesRegex(rv.Indeterminate, 'frame_size_mismatch'):
                rv.load_frames([a, small, a])

    def test_box_off_screen_is_indeterminate(self):
        r = self.measure(triple(self.scene), v=view({'head': 'off', 'feet': (100, 80)}))
        self.assertIn('box_off_screen', r['reason'])

    def test_scale_mismatch_is_indeterminate(self):
        tall = view({'head': (100, 30), 'chest': (100, 50), 'feet': (100, 85), 'shoulder_l': (94, 40), 'shoulder_r': (106, 40)})
        self.assertIn('scale_mismatch', self.measure(triple(self.scene), v=tall)['reason'])

    def test_point_off_screen_and_unavailable(self):
        r = self.measure(triple(self.scene), v=view({'head': (100, 40), 'chest': (100, 55), 'feet': (100, 80),
                                                     'shoulder_l': 'off', 'shoulder_r': None}))
        self.assertEqual(r['points']['shoulder_l']['verdict'], 'off_screen')
        self.assertEqual(r['points']['shoulder_r']['verdict'], 'indeterminate')
        self.assertEqual(r['points']['head']['verdict'], 'visible')

    def test_dark_room_player_is_still_detected(self):
        dark = Scene(dark=True)
        r = self.measure(triple(dark))
        self.assertTrue(r['stats']['dark'])
        self.assertLess(r['stats']['threshold'], 12)
        self.assertEqual(r['verdict'], 'visible')

    def test_dark_box_without_signal_is_never_not_visible(self):
        dark = Scene(dark=True)
        rock = ((80, 20, 120, 95), (6, 6, 6), 1.0)  # dark rock in front of the player: no signal at all
        r = self.measure(triple(dark, [rock]))
        self.assertEqual(r['verdict'], 'indeterminate')
        self.assertIn('dark', r['reason'])

    def test_command_line_measure_and_view(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            frames = triple(self.scene)
            for name, image in zip(('a', 'b', 'a2'), frames):
                png.write(tmp / (name + '.png'), image)
            (tmp / 'view.json').write_text(json.dumps(view()))
            (tmp / 'cal.json').write_text(json.dumps(self.cal, default=list))
            slice_path = tmp / 'slice.jsonl'
            slice_path.write_text('\n'.join(json.dumps(e) for e in (view(tag='x'), {'ev': 'other'}, view(tag='x', available=False))) + '\n')
            self.assertFalse(rv.view_from_slice(slice_path, 'x')['available'])  # the last one wins
            self.assertIsNone(rv.view_from_slice(slice_path, 'y'))
            old = rv.PARAMS['client']
            rv.PARAMS['client'] = CLIENT
            try:
                out = tmp / 'out.json'
                import contextlib
                import io
                buffer = io.StringIO()
                with contextlib.redirect_stdout(buffer):
                    rv.main(['measure', str(tmp / 'a.png'), str(tmp / 'b.png'), str(tmp / 'a2.png'),
                             '--view', str(tmp / 'view.json'), '--view-after', str(tmp / 'view.json'),
                             '--calibration', str(tmp / 'cal.json')])
                result = json.loads(buffer.getvalue())
                self.assertEqual(result['verdict'], 'visible')
                with contextlib.redirect_stdout(io.StringIO()):
                    rv.main(['overlay', str(tmp / 'a.png'), '--view', str(tmp / 'view.json'), '--out', str(out.with_suffix('.png'))])
                self.assertGreater(png.read(out.with_suffix('.png')).width, 0)
            finally:
                rv.PARAMS['client'] = old


@unittest.skipUnless((WAVE1 / 'batch-p55-a-open-1-00/screenshots/a-open-h0.png').is_file(), 'wave 1 frames not on this host')
class Wave1FrameTests(unittest.TestCase):
    """Box mapping on real runner captures. The point pixels below were checked by eye against the figure
    (helmet/neck, boots) in zoomed crops; this guards the mapping against regressions."""

    def mapped(self, label, tag):
        frame = png.read(WAVE1 / f'batch-{label}-00/screenshots/{tag}.png')
        v = rv.view_from_slice(WAVE1_ROUTES / label / 'collected/probe-slice.jsonl', tag)
        g = rv.Geometry(rv.client_origin(frame.width, frame.height, 1920, 1080), (1920, 1080))
        return frame, rv.layout(v, g)

    def test_capture_geometry(self):
        frame = png.read(WAVE1 / 'batch-p55-a-open-1-00/screenshots/a-open-h0.png')
        self.assertEqual((frame.width, frame.height), (1926, 1112))
        self.assertEqual(frame.pixel(500, 10), (50, 150, 250))  # title bar
        self.assertEqual(frame.pixel(1, 500), (255, 255, 255))  # left border
        self.assertNotEqual(frame.pixel(500, 29), (245, 245, 245))  # first client row

    def test_open_ground_points_land_on_the_figure(self):
        _, lay = self.mapped('p55-a-open-1', 'a-open-h0')
        self.assertEqual(tuple(round(v) for v in lay.points['head']), (962, 648))
        self.assertEqual(tuple(round(v) for v in lay.points['feet']), (964, 742))
        self.assertAlmostEqual(lay.height, 94.3, delta=0.5)

    def test_dragonsreach_box_covers_the_boots(self):
        _, lay = self.mapped('p55-c-hall-seed0-A', 'c-hall-h2')
        self.assertEqual(tuple(round(v) for v in lay.points['head']), (965, 654))
        self.assertGreaterEqual(lay.box[3], 766)  # rendered boots at y ~766, 24 px below the feet point


@unittest.skipUnless((ORACLE_EXT_1_SHOTS / 'cal-h0-a2.png').is_file() and ORACLE_EXT_1.is_dir(),
                     'p55-oracle-ext-1 frames not on this host')
class OracleExt1Tests(unittest.TestCase):
    """First in-game marker frames (p55-oracle-ext-1, spike stage 744609f + probe aeaccbd). The expected answers
    are the orchestrator's reading of the frames: open-h3 plainly visible, open-h2 head and shoulders above
    the thatch with the feet under it, stable-h0 under the stable roof."""

    @classmethod
    def setUpClass(cls):
        cls.cal = cls.run_tag('cal-h0')
        assert cls.cal['confirmed'], cls.cal['reason']

    @staticmethod
    def run_tag(tag, calibration=None):
        frames = rv.load_frames([ORACLE_EXT_1_SHOTS / f'{tag}-{s}.png' for s in ('a', 'b', 'a2')])
        slice_path = ORACLE_EXT_1 / 'probe-slice.jsonl'
        v, after = rv.view_from_slice(slice_path, tag), rv.view_from_slice(slice_path, tag + '-post')
        return rv.calibrate(frames, v, after) if calibration is None else rv.measure(frames, v, calibration, after)

    def test_calibration_confirms_although_a_and_a2_differ(self):
        self.assertEqual(len(self.cal['confirmed_points']), 5)
        self.assertGreater(self.cal['stats']['player_change'], 0.3)  # idle animation: the version 1 refusal
        self.assertLess(self.cal['stats']['noise'], 0.05)

    def test_open_h3_visible(self):
        r = self.run_tag('open-h3', self.cal)
        self.assertEqual(r['verdict'], 'visible', r['reason'])
        self.assertTrue(r['restored'])

    def test_open_h2_head_visible_feet_not(self):
        r = self.run_tag('open-h2', self.cal)
        self.assertEqual(r['points']['head']['verdict'], 'visible')
        self.assertEqual(r['points']['feet']['verdict'], 'not_visible')
        self.assertEqual(r['verdict'], 'visible')  # 4 of 5 points

    def test_stable_h0_not_visible(self):
        r = self.run_tag('stable-h0', self.cal)
        self.assertEqual(r['verdict'], 'not_visible', r['reason'])
        self.assertIsNone(r['restored'])  # nothing to restore on screen: unprovable, not failed


if __name__ == '__main__':
    unittest.main()
