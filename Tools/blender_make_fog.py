"""Erzeugt Tools/Import/Fx/T_FogPuff.png: 2x2-Atlas mit vier breiten, weichen Nebelbaenken (Bausche + fraktales Rauschen).

Aufruf: blender.exe -b --factory-startup --python Tools/blender_make_fog.py
"""
import os
import bpy
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Import", "Fx", "T_FogPuff.png")
S = 512
rng = np.random.default_rng(7)


def value_noise(res):
    g = rng.random((res + 1, res + 1))
    x = np.linspace(0, res, S, endpoint=False)
    i = x.astype(int)
    f = x - i
    f = f * f * (3 - 2 * f)
    a = g[np.ix_(i, i)]
    b = g[np.ix_(i, i + 1)]
    c = g[np.ix_(i + 1, i)]
    d = g[np.ix_(i + 1, i + 1)]
    fx = f[None, :]
    fy = f[:, None]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm():
    n = sum(value_noise(r) * w for r, w in ((4, 0.5), (8, 0.25), (16, 0.15), (32, 0.1)))
    return (n - n.min()) / (n.max() - n.min())


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


v, u = np.mgrid[0:S, 0:S] / S
cells = []
for k in range(4):
    dens = np.zeros((S, S))
    # breite Wolkenbank aus runden Bauschen (Cartoon-Nebel)
    n_b = 6 + k
    for j in range(n_b):
        cx = 0.14 + 0.72 * (j + rng.random() * 0.6) / n_b
        cy = 0.52 + rng.uniform(-0.1, 0.06)
        r = rng.uniform(0.13, 0.22)
        dist = np.sqrt((u - cx) ** 2 + ((v - cy) * 1.25) ** 2)
        dens = 1 - (1 - dens) * (1 - 0.8 * smooth(r * 1.2, r * 0.05, dist))
    band = smooth(0.34, 0.1, np.sqrt(((u - 0.5) / 1.6) ** 2 + ((v - 0.6) * 1.8) ** 2))
    dens = np.maximum(dens, band * 0.8)
    n = fbm()
    d = dens * (0.3 + 0.8 * n) ** 1.3
    # Rand der Zelle sicher auf 0
    edge = smooth(0.0, 0.08, u) * smooth(1.0, 0.92, u) * smooth(0.0, 0.1, v) * smooth(1.0, 0.9, v)
    cells.append(np.clip(d * edge * 1.05, 0, 1))

atlas = np.zeros((S * 2, S * 2))
atlas[:S, :S] = cells[0]
atlas[:S, S:] = cells[1]
atlas[S:, :S] = cells[2]
atlas[S:, S:] = cells[3]

img = bpy.data.images.new("fog", S * 2, S * 2, alpha=True)
px = np.ones((S * 2, S * 2, 4), dtype=np.float32)
# Blender-Bilder beginnen unten links
flipped = atlas[::-1]
px[..., 0] = flipped
px[..., 1] = flipped
px[..., 2] = flipped
img.pixels.foreach_set(px.ravel())
img.filepath_raw = OUT
img.file_format = "PNG"
img.save()
print("FOG OK", atlas.mean())
