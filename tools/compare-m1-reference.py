# Compares island silhouettes of native renders vs the HTML prototype (Hausdorff distance of outlines).
# Text glyphs differ by renderer/font, so only the shape outline is held to the <= 1 px budget.
import sys
from pathlib import Path
import numpy as np
from PIL import Image

root = Path('artifacts/m1-validation')
pairs = {'dormant': 'dormant', 'compact': 'compact-reduced-0.1', 'expanded-now': 'expanded-now-playing',
         'expanded-telemetry': 'expanded-telemetry', 'expanded-tray': 'expanded-drop-tray',
         'hud-volume': 'hud-volume', 'hud-brightness': 'hud-brightness', 'hud-pomodoro': 'hud-pomodoro'}

def outline(path):
    image = Image.open(path).convert('RGBA')
    gray = np.asarray(Image.alpha_composite(Image.new('RGBA', image.size, 'white'), image).convert('L'))
    dark = gray < 128  # 50% coverage of the near-black surface; the drop shadow on white stays far lighter
    # The prototype draws a dimension label below the island; keep rows connected to y=0.
    height = next((y for y in range(dark.shape[0]) if not dark[y].any()), dark.shape[0])
    dark = dark[:height].copy()
    for row in dark:  # the silhouette is row-convex; fill holes punched by light content (art, text)
        xs = np.flatnonzero(row)
        row[xs[0]:xs[-1] + 1] = True
    padded = np.pad(dark, 1)
    inner = padded[:-2, 1:-1] & padded[2:, 1:-1] & padded[1:-1, :-2] & padded[1:-1, 2:]
    edge = dark & ~inner
    edge[0] = False  # the top row is the monitor edge, not part of the drawn outline
    return np.argwhere(edge).astype(float), height

def hausdorff(a, b):
    d = np.sqrt(((a[:, None, :] - b[None, :, :]) ** 2).sum(-1))
    return max(d.min(1).max(), d.min(0).max())

failed = False
for dpi in ('96dpi', '144dpi'):
    for ref, native in pairs.items():
        (a, ha), (b, hb) = outline(root / 'reference' / dpi / f'{ref}.png'), outline(root / 'render' / dpi / f'{native}.png')
        # Crop origins differ horizontally; take the best alignment near the center offset.
        base = round(a[:, 1].mean() - b[:, 1].mean())
        worst = min(hausdorff(a, b + [0, base + s]) for s in np.arange(-1, 1.01, 0.25))
        ok = abs(ha - hb) <= 1 and worst <= 1.0 + 1e-6
        failed |= not ok
        print(f"{'PASS' if ok else 'FAIL'} {dpi} {ref}: height ref={ha} native={hb}, outline delta={worst:.2f}px")
sys.exit(1 if failed else 0)
