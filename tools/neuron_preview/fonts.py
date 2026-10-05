"""Font table shared by the preview generator and renderer.

Mirrors the built-in fonts the Neuron screens use (sizes are points at the
150 DPI fontconvert.py renders with; line height/ascender come from the
generated EpdFontData headers).
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'lib', 'EpdFont', 'builtinFonts', 'source')

# name: (fontId macro, size pt, line height, ascender, {style: ttf})
FONTS = {
    'SMALL': ('SMALL_FONT_ID', 8, 23, 18, {'R': 'NotoSans/NotoSans-Regular.ttf'}),
    'UI10': ('UI_10_FONT_ID', 10, 24, 20, {'R': 'Ubuntu/Ubuntu-Regular.ttf', 'B': 'Ubuntu/Ubuntu-Bold.ttf'}),
    'UI12': ('UI_12_FONT_ID', 12, 29, 24, {'R': 'Ubuntu/Ubuntu-Regular.ttf', 'B': 'Ubuntu/Ubuntu-Bold.ttf'}),
    'SANS18': ('NOTOSANS_18_FONT_ID', 18, 51, 41,
               {'R': 'NotoSans/NotoSans-Regular.ttf', 'B': 'NotoSans/NotoSans-Bold.ttf'}),
    'SERIF12': ('NOTOSERIF_12_FONT_ID', 12, 34, 27,
                {'R': 'NotoSerif/NotoSerif-Regular.ttf', 'B': 'NotoSerif/NotoSerif-Bold.ttf',
                 'I': 'NotoSerif/NotoSerif-Italic.ttf'}),
}


def ttf(name, style):
    files = FONTS[name][4]
    return os.path.join(SRC, files.get(style, files['R']))


def ppem(name):
    return FONTS[name][1] * 150 / 72
