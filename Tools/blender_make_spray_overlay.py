"""Erzeugt Tools/Import/UI/Spray Overlay.png: schwarzer Spruehnebel fuer den unteren Bildschirmrand (Spraydose).
Unten dicht und weich, nach oben ausfransend mit Spruehpunkten; transparenter Hintergrund.

Aufruf: blender.exe -b --factory-startup --python Tools/blender_make_spray_overlay.py
"""
import os
import bpy
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Import", "UI", "Spray Overlay.png")
W, H = 1024, 640
rng = np.random.default_rng(11)
y, x = np.mgrid[0:H, 0:W]
v = y / (H - 1)          # 0 oben, 1 unten
u = x / (W - 1)


def value_noise(res_x, res_y):
    g = rng.random((res_y + 2, res_x + 2))
    fx = u * res_x
    fy = v * res_y
    ix = fx.astype(int)
    iy = fy.astype(int)
    tx = fx - ix
    ty = fy - iy
    tx = tx * tx * (3 - 2 * tx)
    ty = ty * ty * (3 - 2 * ty)
    a = g[iy, ix]
    b = g[iy, ix + 1]
    c = g[iy + 1, ix]
    d = g[iy + 1, ix + 1]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


n = 0.5 * value_noise(6, 3) + 0.3 * value_noise(14, 7) + 0.2 * value_noise(40, 20)
# Rand des dichten Nebels: wellig, etwa auf 55 % Hoehe
edge = 0.45 + 0.22 * (n - 0.5) * 2
fog = np.clip((v - edge) / 0.28, 0, 1)
fog = fog * fog * (3 - 2 * fog)
alpha = fog * (0.82 + 0.18 * n)
# Spruehpunkte: oberhalb des Nebels dichter werdend nach unten
count = 2600
px = rng.random(count) * W
py = (rng.random(count) ** 0.55) * H
pr = 1.2 + rng.random(count) ** 3 * 7.0
dots = np.zeros((H, W))
for cx, cy, r in zip(px, py, pr):
    x0, x1 = int(max(0, cx - r - 2)), int(min(W, cx + r + 3))
    y0, y1 = int(max(0, cy - r - 2)), int(min(H, cy + r + 3))
    if x0 >= x1 or y0 >= y1:
        continue
    dd = np.sqrt((x[y0:y1, x0:x1] - cx) ** 2 + (y[y0:y1, x0:x1] - cy) ** 2)
    dots[y0:y1, x0:x1] = np.maximum(dots[y0:y1, x0:x1], np.clip(r + 0.8 - dd, 0, 1))
alpha = np.clip(np.maximum(alpha, dots * 0.95), 0, 1)

img = bpy.data.images.new("spray_overlay", W, H, alpha=True)
pxl = np.zeros((H, W, 4), dtype=np.float32)
pxl[..., 3] = alpha
img.pixels.foreach_set(pxl[::-1].ravel())
img.filepath_raw = OUT
img.file_format = "PNG"
img.save()
print("OVERLAY OK", OUT)
