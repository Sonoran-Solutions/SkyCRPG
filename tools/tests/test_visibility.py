"""Rendered-visibility tool (tools/visibility): PNG reader, geometry, verdict rules on synthetic frames.

No game, no third-party packages. Wave 1 frames are read only if they exist on this host (skipped otherwise).
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
        # Player figure in client pixels: a 16 px wide body from y 28 to 86 (head point 40, feet point 80).
        self.figure = (92, 28, 108, 86)

    def frame(self, player=True, occluders=(), shift=0):
        """occluders: (rect in client px, colour, alpha) drawn over everything, in order."""
        rows = []
        for y in range(self.h):
            row = bytearray()
            for x in range(self.w):
                px = self.bg[y][(x + shift) % self.w] if TITLE <= y < self.h - BORDER and BORDER <= x < self.w - BORDER else self.bg[y][x]
                cx, cy = x - BORDER, y - TITLE
                if player and self.figure[0] <= cx < self.figure[2] and self.figure[1] <= cy < self.figure[3]:
                    px = self.player_colour
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


def triple(scene, occluders=(), restored=True, shift_b=0):
    a = scene.frame(True, occluders)
    return [a, scene.frame(False, occluders, shift=shift_b), a if restored else scene.frame(False, occluders)]


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

    def test_lower_body_occluded_is_partly_with_per_point_answers(self):
        r = self.measure(triple(self.scene, [((80, 60, 120, 95), (90, 90, 90), 1.0)]))
        self.assertEqual(r['verdict'], 'partly')
        self.assertEqual(r['points']['head']['verdict'], 'visible')
        self.assertEqual(r['points']['feet']['verdict'], 'not_visible')

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
        a, b, _ = triple(self.scene)
        windy = [jitter(a, 0.06, 1), jitter(b, 0.06, 2), jitter(a, 0.06, 3)]  # wave 1 exterior pairs: 0.05-0.06
        r = self.measure(windy)
        self.assertEqual(r['verdict'], 'visible', r['reason'])
        storm = [jitter(a, 0.25, 1), jitter(b, 0.25, 2), jitter(a, 0.25, 3)]
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


if __name__ == '__main__':
    unittest.main()
