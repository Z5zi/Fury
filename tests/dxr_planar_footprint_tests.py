#!/usr/bin/env python3
"""Source-guarded DXR footprint math checks; these do not execute a GPU shader.

Run with: python3 tests/dxr_planar_footprint_tests.py
"""

import math
from pathlib import Path
import random
import re
import unittest


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalize(a):
    length = math.sqrt(dot(a, a))
    return tuple(x / length for x in a)


def planar_basis(n):
    x, y, z = map(abs, n)
    if y >= x and y >= z:
        return (1, 0, 0), (0, 0, 1)
    if z >= x:
        return (1, 0, 0), (0, -1, 0)
    return (0, 0, 1), (0, -1, 0)


def projected_major_axis(n, direction, scale, width, height):
    """Independent ray-plane projection followed by the exact UV Jacobian norm."""
    reference = (0, 1, 0) if abs(direction[1]) < .99 else (1, 0, 0)
    a = normalize(cross(reference, direction))
    b = cross(direction, a)
    cosine = dot(n, direction)
    u, v = planar_basis(n)

    def project(axis):
        distance = dot(n, axis) / cosine
        world = tuple(x - d * distance for x, d in zip(axis, direction))
        return dot(world, u) * scale * width, dot(world, v) * scale * height

    dx, dy = project(a), project(b)
    aa, ab, bb = dot(dx, dx), dot(dx, dy), dot(dy, dy)
    return math.sqrt(.5 * (aa + bb + math.hypot(aa - bb, 2 * ab)))


class DxrPlanarFootprintTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        shader = (Path(__file__).resolve().parents[1] / 'engine/shaders/dx12.hlsl').read_text()
        compact = re.sub(r'\s+', '', shader)
        # Keep the numerical model tied to the actual shader branch. A different
        # implementation needs an updated model, not a silently passing test.
        match = re.search(
            r'floatdensity=m\.animation\.w>0\?m\.animation\.w/'
            r'max\(abs\(dot\(hit\.geometric_normal,direction\)\),([0-9.eE+-]+)\):',
            compact)
        if match is None:
            raise AssertionError('World-planar density must include geometric grazing correction')
        cls.cosine_floor = float(match.group(1))
        expected_mesh = ('max(length(b.uv-a.uv)/max(length(b.position-a.position),1e-5),'
                         'length(c.uv-a.uv)/max(length(c.position-a.position),1e-5));')
        if not compact[match.end():].startswith(expected_mesh):
            raise AssertionError('Mesh-UV density changed; review its separate regression contract')

    def density(self, scale, n, direction):
        return scale / max(abs(dot(n, direction)), self.cosine_floor)

    def test_expected_mip_increase(self):
        n = (0, 1, 0)
        for cosine in (1, .5, .05, .001, .0001):
            direction = (math.sqrt(1 - cosine * cosine), -cosine, 0)
            density = self.density(2, n, direction)
            self.assertAlmostEqual(math.log2(density / 2), -math.log2(cosine), places=12)
        self.assertAlmostEqual(math.log2(self.density(2, n, (math.sqrt(.9975), -.05, 0)) / 2),
                               4.321928094887363, places=12)

    def test_backfaces_and_near_parallel_limit(self):
        self.assertEqual(self.cosine_floor, 1e-4)
        for cosine in (1, .05, .0001, .0000001, 0):
            direction = (math.sqrt(1 - cosine * cosine), cosine, 0)
            density = self.density(3, (0, 1, 0), direction)
            self.assertEqual(density, self.density(3, (0, -1, 0), direction))
            self.assertTrue(math.isfinite(density))
            self.assertLessEqual(density, 3 / self.cosine_floor)

    def test_bounds_projected_footprint(self):
        rng = random.Random(1394)
        cases = 0
        for _ in range(20000):
            n = normalize(tuple(rng.uniform(-1, 1) for _ in range(3)))
            direction = normalize(tuple(rng.uniform(-1, 1) for _ in range(3)))
            if abs(dot(n, direction)) < self.cosine_floor:
                continue
            scale = 10 ** rng.uniform(-2, 2)
            width, height = rng.choice((64, 256, 1024)), rng.choice((64, 256, 1024))
            exact = projected_major_axis(n, direction, scale, width, height)
            bound = self.density(scale, n, direction) * max(width, height)
            self.assertLessEqual(exact, bound * (1 + 1e-10))
            # Mirroring the scene and ray together preserves the UV footprint.
            for signs in ((-1, 1, 1), (1, -1, 1), (1, 1, -1)):
                mirrored_n = tuple(x * s for x, s in zip(n, signs))
                mirrored_ray = tuple(x * s for x, s in zip(direction, signs))
                self.assertEqual(bound, self.density(scale, mirrored_n, mirrored_ray) * max(width, height))
            cases += 1
        self.assertGreater(cases, 19000)


if __name__ == '__main__':
    unittest.main(verbosity=2)
