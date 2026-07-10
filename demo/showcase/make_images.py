#!/usr/bin/env python3
"""Procedural showcase images spanning Lumen's mood space.

The key detector maps average hue -> circle-of-fifths root, and saturation +
luminance -> scale family (washed-out -> pentatonics; vivid & dark -> blues /
harmonic minor; otherwise diatonic modes along a dark -> bright axis). These
eight images are chosen to hit distinct corners of that space, plus a
high/low-contrast pair to exercise the rhythm-density axis. All procedural —
no third-party or copyrighted material.

Deterministic: fixed PRNG seeds, no timestamps. Regenerate with
`python3 demo/showcase/make_images.py`.
"""
import colorsys
import math
import random
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
SIZE = 512


def hsv(h, s, v):
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, max(0.0, min(1.0, s)), max(0.0, min(1.0, v)))
    return int(r * 255), int(g * 255), int(b * 255)


def render(name, pixel):
    img = Image.new("RGB", (SIZE, SIZE))
    px = img.load()
    for y in range(SIZE):
        for x in range(SIZE):
            px[x, y] = pixel(x / (SIZE - 1), y / (SIZE - 1))
    img.save(HERE / f"{name}.png")
    print(name)


def main():
    rng = random.Random(51)
    noise = [[rng.random() for _ in range(65)] for _ in range(65)]

    def smooth_noise(u, v, scale=8.0):
        gu, gv = u * scale, v * scale
        iu, iv = int(gu), int(gv)
        fu, fv = gu - iu, gv - iv
        a = noise[iv][iu]
        b = noise[iv][iu + 1]
        c = noise[iv + 1][iu]
        d = noise[iv + 1][iu + 1]
        return a * (1 - fu) * (1 - fv) + b * fu * (1 - fv) + c * (1 - fu) * fv + d * fu * fv

    # 1. Bright + saturated warm gradient -> brightest corner (Lydian/Ionian).
    render("01-sunrise-bright",
           lambda u, v: hsv(0.10 + 0.06 * v, 0.85 - 0.25 * v, 0.95 - 0.25 * v))

    # 2. Vivid and dark magenta/violet -> blues / harmonic minor corner.
    render("02-neon-dusk",
           lambda u, v: hsv(0.83 + 0.05 * u, 0.9, 0.22 + 0.18 * smooth_noise(u, v, 6)))

    # 3. Near-grayscale fog -> washed-out pentatonic fallback.
    render("03-fog-washedout",
           lambda u, v: hsv(0.55, 0.05, 0.55 + 0.25 * smooth_noise(u, v, 4)))

    # 4. Dark forest greens -> dark diatonic modes (Phrygian/Aeolian).
    render("04-forest-dark",
           lambda u, v: hsv(0.33 + 0.04 * smooth_noise(u, v, 10), 0.55, 0.18 + 0.2 * v))

    # 5. Mid-lum teal ocean -> dusky middle modes (Dorian).
    render("05-ocean-mid",
           lambda u, v: hsv(0.50 + 0.03 * math.sin(6.28 * v * 3 + 2 * u),
                            0.55, 0.45 + 0.15 * math.sin(6.28 * (v * 2 + 0.3 * smooth_noise(u, v, 5)))))

    # 6. Warm amber field -> warm/bright modes (Mixolydian/Ionian).
    render("06-amber-warm",
           lambda u, v: hsv(0.07, 0.6 + 0.15 * smooth_noise(u, v, 7), 0.6 + 0.2 * u))

    # 7. High-contrast vivid red/black blocks -> vivid-dark family, busy
    #    per-region contrast for the DENSITY axis.
    def ember(u, v):
        cell = (int(u * 8) + int(v * 8)) % 2
        flame = smooth_noise(u, v, 12)
        return hsv(0.00 + 0.03 * flame, 0.95, 0.75 if cell else 0.08 + 0.1 * flame)
    render("07-ember-highcontrast", ember)

    # 8. Smooth pastel low-contrast wash -> quiet end of the density axis.
    render("08-pastel-lowcontrast",
           lambda u, v: hsv(0.60 + 0.1 * u, 0.22, 0.78 + 0.06 * v))


if __name__ == "__main__":
    main()
