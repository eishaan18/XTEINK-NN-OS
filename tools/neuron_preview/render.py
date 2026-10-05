"""Replays the draw-op logs from out_preview into PNG screenshots."""
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

from fonts import FONTS, ROOT, ppem, ttf

W, H = 480, 800
_font_cache = {}


def font(name, style):
    key = (name, style)
    if key not in _font_cache:
        _font_cache[key] = ImageFont.truetype(ttf(name, style), round(ppem(name)))
    return _font_cache[key]


def load_logo():
    src = open(os.path.join(ROOT, 'src', 'images', 'NeuronLogo.h')).read()
    size = int(re.search(r'NEURON_LOGO_SIZE = (\d+)', src).group(1))
    data = [int(v, 16) for v in re.findall(r'0x([0-9A-F]{2})', src.split('NeuronLogo[]')[1])]
    img = Image.new('L', (size, size), 255)
    px = img.load()
    for i, v in enumerate(data):
        y, x = divmod(i * 8, size)
        for b in range(8):
            px[x + b, y] = 255 if (v >> (7 - b)) & 1 else 0
    return img.rotate(-90)


def color(flag):
    return 0 if int(flag) else 255


def render(path):
    img = Image.new('L', (W, H), 255)
    d = ImageDraw.Draw(img)
    for line in open(path, encoding='utf-8'):
        parts = line.rstrip('\n').split(' ')
        op = parts[0]
        if op == 'clear':
            d.rectangle([0, 0, W, H], fill=255 if int(parts[1]) else 0)
        elif op == 'invert':
            img = Image.eval(img, lambda v: 255 - v)
            d = ImageDraw.Draw(img)
        elif op == 'frect':
            x, y, w, h, b = map(int, parts[1:6])
            if w > 0 and h > 0:
                d.rectangle([x, y, x + w - 1, y + h - 1], fill=color(b))
        elif op == 'rect':
            x, y, w, h, lw, b = map(int, parts[1:7])
            d.rectangle([x, y, x + w - 1, y + h - 1], outline=color(b), width=lw)
        elif op == 'rrect':
            x, y, w, h, lw, r, b = map(int, parts[1:8])
            d.rounded_rectangle([x, y, x + w - 1, y + h - 1], radius=r, outline=color(b), width=lw)
        elif op == 'frrect':
            x, y, w, h, r, b = map(int, parts[1:7])
            d.rounded_rectangle([x, y, x + w - 1, y + h - 1], radius=r, fill=color(b))
        elif op == 'line':
            x1, y1, x2, y2, lw, b = map(int, parts[1:7])
            d.line([x1, y1, x2, y2], fill=color(b), width=lw)
        elif op == 'poly':
            b, n = int(parts[1]), int(parts[2])
            pts = [(int(parts[3 + 2 * i]), int(parts[4 + 2 * i])) for i in range(n)]
            d.polygon(pts, fill=color(b))
        elif op == 'text':
            name, style, x, y, b = parts[1], parts[2], int(parts[3]), int(parts[4]), parts[5]
            text = ' '.join(parts[7:])
            d.text((x, y), text, font=font(name, style), fill=color(b), anchor='ls')
        elif op == 'image':
            x, y, w, h = map(int, parts[1:5])
            img.paste(load_logo().resize((w, h)), (x, y))
    out = os.path.splitext(path)[0] + '.png'
    framed = Image.new('L', (W + 24, H + 24), 40)
    framed.paste(img, (12, 12))
    framed.save(out)
    print('rendered', out)


if __name__ == '__main__':
    for p in sys.argv[1:]:
        render(p)
