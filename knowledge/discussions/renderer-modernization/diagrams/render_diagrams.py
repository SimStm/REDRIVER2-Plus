"""Regenerate discussion images with Python and Pillow; no game assets required.

Run: python knowledge/discussions/renderer-modernization/diagrams/render_diagrams.py
Outputs are written beside this script. Override DIAGRAM_FONT with a TrueType
font path on platforms without Segoe UI or DejaVu Sans.
"""

import math
import os
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
WIDTH = 1500
INK = '#162B3A'
MUTED = '#4C6170'
LINE = '#6D8291'
ACCENT = '#0A6570'


def font(size):
    for path in [os.environ.get('DIAGRAM_FONT'), 'C:/Windows/Fonts/segoeui.ttf',
                 '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf']:
        if path and Path(path).is_file():
            return ImageFont.truetype(path, size)
    raise RuntimeError('Set DIAGRAM_FONT to a TrueType font file.')


def text(draw, xy, value, size=25, fill=INK, max_width=None):
    face = font(size)
    if max_width is not None:
        assert draw.textlength(value, font=face) <= max_width, value
    draw.text(xy, value, font=face, fill=fill)


def box(draw, rect, title, lines, proposed=False):
    x, y, right, bottom = rect
    draw.rounded_rectangle(rect, radius=12,
                           fill='#EAF5F5' if proposed else '#F2F5F7',
                           outline=ACCENT if proposed else '#BCC9D2', width=2)
    text(draw, (x + 24, y + 18), title, 29, max_width=right-x-48)
    for row, line in enumerate(lines):
        text(draw, (x + 24, y + 62 + row * 32), line, 24,
             MUTED, max_width=right-x-48)
    assert y + 62 + len(lines) * 32 <= bottom, title


def arrow(draw, points):
    draw.line(points, fill=LINE, width=4)
    end, prev = points[-1], points[-2]
    angle = math.atan2(end[1]-prev[1], end[0]-prev[0])
    sides = [(end[0]-15*math.cos(angle+d), end[1]-15*math.sin(angle+d))
             for d in (-0.5, 0.5)]
    draw.polygon([end, *sides], fill=LINE)


def canvas(height, title, subtitle):
    image = Image.new('RGB', (WIDTH, height), 'white')
    draw = ImageDraw.Draw(image)
    text(draw, (50, 24), title, 39)
    text(draw, (50, 78), subtitle, 23, MUTED)
    return image, draw


def architecture():
    image, draw = canvas(1110, 'Renderer modernization: proposed boundaries',
                         'DISCUSSION / NOT IMPLEMENTED  |  2026-09-15  |  Teal marks proposed graphics work')
    box(draw, (480, 145, 1030, 285), 'Game simulation and world data',
        ['Physics, missions, traffic, object state', 'Preserve existing gameplay contracts'])
    for dest in (255, 735, 1225):
        arrow(draw, [(755, 285), (755, 320), (dest, 320), (dest, 355)])
    box(draw, (50, 355, 460, 530), 'PsyCross services',
        ['Audio, input and other', 'compatibility interfaces', 'Remain useful independently'])
    box(draw, (520, 355, 950, 530), 'Legacy graphics adapter',
        ['Psy-Q primitives, PGXP,', 'VRAM/palettes and blending', 'Retain compatibility semantics'])
    box(draw, (1000, 355, 1450, 530), 'Modern scene adapter',
        ['Meshes, transforms, materials,', 'lights, camera and visibility', 'Game-specific data stays here'], True)
    for origin in (735, 1225):
        arrow(draw, [(origin, 530), (origin, 630)])
    text(draw, (55, 575), 'No graphics rewrite is needed', 24, MUTED)
    text(draw, (55, 610), 'for these retained services.', 24, MUTED)
    box(draw, (500, 630, 1450, 805), 'Frame integration and small shared backend boundary',
        ['Explicit passes, resources, camera/depth conventions and presentation',
         'First experiment: one OpenGL context and mutual occlusion',
         'Depth/prepass strategy and module ownership remain open'], True)
    arrow(draw, [(975, 805), (975, 895)])
    box(draw, (650, 895, 1300, 1035), 'One selected graphics backend per run',
        ['OpenGL first if suitable; Vulkan/Metal later',
         'Candidate backends, not delivered platform support'], True)
    text(draw, (50, 1070), 'Source: discussion index and exploration sequence. Rendering paths must share a coherent frame.', 22, MUTED)
    image.save(OUT / 'proposed-architecture.png')


def sequence():
    image, draw = canvas(1370, 'Recommended sequence: prove one boundary at a time',
                         'EXPLORING / NOT A ROADMAP  |  No experiment has been executed  |  2026-09-15')
    stages = [
        ('0-2  Define target, measure baseline, specify contracts',
         ['Choose the first visible result and hardware; trace one object to the renderer.',
          'Gate: reproducible comparison and an explicit scene/depth boundary.']),
        ('3  Prove a synthetic unlit mesh in the original world',
         ['Share camera and depth; verify occlusion, clipping, HUD and legacy state.',
          'Gate: reliable hybrid rendering and a toggle restoring the original output.']),
        ('4-5  Import one static asset; establish reference PBR',
         ['Bound the asset format; add normals, colour-space handling and one light.',
          'Gate: correct material response, resource lifetime and measured cost.']),
        ('6-7  Expand lighting and select a measured pipeline',
         ['Add shadows and AO separately; compare Forward with the relevant candidate.',
          'Gate: documented coverage and one justified initial production pipeline.']),
        ('8  Prove another backend with the same test scene',
         ['Port resources, shaders, synchronization, legacy drawing and presentation.',
          'Gate: image parity and actual target testing; review feasibility early if required.']),
        ('9  Expand models and world systems by category',
         ['Static scenery before deforming vehicles/characters; playable maps need more data.',
          'Gate: collision, streaming and gameplay validated for each adopted scope.']),
    ]
    for i, (title, lines) in enumerate(stages):
        y = 150 + i * 180
        if i:
            arrow(draw, [(750, y - 30), (750, y)])
        box(draw, (75, y, 1425, y + 150), title, lines, True)
    text(draw, (75, 1270), 'Before roadmap adoption: agree on the target, compatibility requirements and first bounded experiment.', 24, INK)
    text(draw, (75, 1310), 'Suggested first scope: step 3 with only the necessary baseline and contract work from steps 0-2.', 24, MUTED)
    image.save(OUT / 'exploration-sequence.png')


if __name__ == '__main__':
    architecture()
    sequence()
    print('Rendered proposed-architecture.png and exploration-sequence.png')
