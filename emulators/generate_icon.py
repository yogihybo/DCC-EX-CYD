"""
Generate an LVGL 9 ARGB8888 C icon from a PIL-drawn image.

Usage:
    python generate_icon.py [--name NAME] [--size WxH] [--out PATH]

Defaults:
    --name  train_icon
    --size  30x30
    --out   ../src/<name>.c
"""

import argparse
import os
from PIL import Image, ImageDraw


def draw_train(draw: ImageDraw.ImageDraw, width: int, height: int, color=255, lw=2):
    sx = width  / 30
    sy = height / 30

    def s(x, y):
        return (x * sx, y * sy)

    draw.rounded_rectangle([s(4, 4), s(25, 22)], radius=4 * sx, outline=color, width=lw)
    draw.rounded_rectangle([s(7, 2), s(22, 4)],  radius=1 * sx, outline=color, width=lw)
    draw.rounded_rectangle([s(7, 8), s(13, 14)], radius=2 * sx, outline=color, width=lw)
    draw.rounded_rectangle([s(16, 8), s(22, 14)],radius=2 * sx, outline=color, width=lw)
    draw.ellipse([s(7, 17),  s(10, 20)], outline=color, width=lw)
    draw.ellipse([s(19, 17), s(22, 20)], outline=color, width=lw)
    draw.line([s(8, 22),  s(8, 24)],  fill=color, width=lw)
    draw.line([s(21, 22), s(21, 24)], fill=color, width=lw)
    draw.line([s(6, 24),  s(23, 24)], fill=color, width=lw)
    draw.line([s(10, 24), s(6, 29)],  fill=color, width=lw)
    draw.line([s(19, 24), s(23, 29)], fill=color, width=lw)


def generate(name: str, width: int, height: int, out_path: str):
    img = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(img)
    draw_train(draw, width, height)

    pixels = list(img.getdata())

    rows = []
    for i in range(0, len(pixels), width):
        row = pixels[i:i + width]
        # LVGL 9 ARGB8888 in memory: [B, G, R, A] (little-endian)
        rows.append("    " + ", ".join(f"0xff, 0xff, 0xff, 0x{p:02x}" for p in row) + ",")

    c_array = "const uint8_t {}_map[] = {{\n{}\n}};".format(name, "\n".join(rows))

    c_code = f"""\
#include "lvgl.h"

{c_array}

const lv_image_dsc_t {name} = {{
  .header = {{
    .magic  = LV_IMAGE_HEADER_MAGIC,
    .cf     = LV_COLOR_FORMAT_ARGB8888,
    .flags  = 0,
    .w      = {width},
    .h      = {height},
    .stride = {width * 4},
  }},
  .data_size = sizeof({name}_map),
  .data      = {name}_map,
}};
"""

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w") as f:
        f.write(c_code)

    print(f"Generated {out_path}  ({width}x{height}, {len(pixels)*4} bytes)")


if __name__ == "__main__":
    repo_src = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")

    parser = argparse.ArgumentParser(description="Generate an LVGL 9 ARGB8888 C icon.")
    parser.add_argument("--name", default="train_icon", help="C variable name (default: train_icon)")
    parser.add_argument("--size", default="30x30",      help="WxH in pixels (default: 30x30)")
    parser.add_argument("--out",  default=None,         help="Output .c file path (default: ../src/<name>.c)")
    args = parser.parse_args()

    w, h = (int(v) for v in args.size.lower().split("x"))
    out  = args.out or os.path.join(repo_src, f"{args.name}.c")

    generate(args.name, w, h, out)
