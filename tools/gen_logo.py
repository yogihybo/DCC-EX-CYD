#!/usr/bin/env python3
"""
Generate logo.c — filled icon style badge.
Strategy: draw everything in grayscale (L mode), use as A8 alpha channel.
  255 = opaque (recolored by LVGL)   0 = transparent (shows through)
The disc is solid opaque; text/design are punched out as transparent cutouts.
"""

import os, math
from PIL import Image, ImageDraw, ImageFont

SCALE  = 4
SIZE   = 200
SZ     = SIZE * SCALE
cx = cy = SZ // 2
R  = cx - SCALE * 3

alpha = Image.new("L", (SZ, SZ), 0)   # starts fully transparent
draw  = ImageDraw.Draw(alpha)

# ── solid disc ────────────────────────────────────────────────────────────────
draw.ellipse([cx-R, cy-R, cx+R, cy+R], fill=255)

# ── ring cutouts (two thin grooves in the disc) ────────────────────────────────
def ring_cutout(r, w):
    draw.ellipse([cx-r, cy-r, cx+r, cy+r], outline=0, width=w)

ring_cutout(R - SCALE*1,  SCALE*3)     # outer groove
ring_cutout(R - SCALE*10, SCALE*2)     # inner groove

# ── "DCC" text cutout ─────────────────────────────────────────────────────────
try:
    font_dcc = ImageFont.truetype("arialbd.ttf", SCALE * 32)
except:
    font_dcc = ImageFont.load_default(size=SCALE * 32)

bbox = draw.textbbox((0, 0), "DCC", font=font_dcc)
tw = bbox[2] - bbox[0]
th = bbox[3] - bbox[1]
draw.text((cx - tw//2 - bbox[0], cy - R//2 + SCALE*4 - bbox[1]),
          "DCC", font=font_dcc, fill=0)

# ── controller body (white block, knob/buttons punched out) ───────────────────
body_x  = cx - SCALE*34
body_y  = cy - SCALE*4
body_w  = SCALE*68
body_h  = SCALE*46
body_r  = SCALE*7

# body outline cutout (just the border stroke, interior stays opaque)
draw.rounded_rectangle([body_x, body_y, body_x+body_w, body_y+body_h],
                        radius=body_r, outline=0, width=SCALE*2)

# knob ring cutout
kx, ky, kr = cx, cy + SCALE*8, SCALE*12
draw.ellipse([kx-kr, ky-kr, kx+kr, ky+kr], outline=0, width=SCALE*2)

# knob centre dot cutout
draw.ellipse([kx-SCALE*3, ky-SCALE*3, kx+SCALE*3, ky+SCALE*3], fill=0)

# tick marks on knob (cutout lines)
for angle_deg in [-35, 0, 35]:
    a = math.radians(angle_deg - 90)
    x0 = int(kx + (kr + SCALE)   * math.cos(a))
    y0 = int(ky + (kr + SCALE)   * math.sin(a))
    x1 = int(kx + (kr + SCALE*4) * math.cos(a))
    y1 = int(ky + (kr + SCALE*4) * math.sin(a))
    draw.line([x0, y0, x1, y1], fill=0, width=SCALE)

# three buttons at bottom of body (cutout rectangles)
btn_y = body_y + body_h - SCALE*13
for bx2 in [cx - SCALE*22, cx - SCALE*4, cx + SCALE*14]:
    draw.rounded_rectangle([bx2, btn_y, bx2+SCALE*9, btn_y+SCALE*7],
                            radius=SCALE*2, fill=0)

# ── arc text helper (draws cutout characters along a circle) ──────────────────
def arc_text(text, arc_r, mid_deg, font_size, flip=False):
    try:
        fnt = ImageFont.truetype("arial.ttf", font_size)
    except:
        fnt = ImageFont.load_default(size=font_size)

    tmp_draw = ImageDraw.Draw(alpha)
    # Cumulative positions using full-string measurement to capture kerning
    total_w = tmp_draw.textlength(text, font=fnt)
    positions = [tmp_draw.textlength(text[:i], font=fnt) for i in range(len(text))]
    char_widths = [tmp_draw.textlength(text[i:i+1], font=fnt) for i in range(len(text))]
    offsets = [positions[i] + char_widths[i]/2 - total_w/2 for i in range(len(text))]

    chars = list(reversed(text)) if flip else list(text)
    for ch, off in zip(chars, offsets):
        char_deg = mid_deg + math.degrees(off / arc_r)
        a = math.radians(char_deg - 90)
        tx = int(cx + arc_r * math.cos(a))
        ty = int(cy + arc_r * math.sin(a))

        cb  = tmp_draw.textbbox((0,0), ch, font=fnt)
        cw2 = cb[2] - cb[0]; ch2 = cb[3] - cb[1]
        char_img = Image.new("L", (cw2+6, ch2+6), 255)  # white = keep opaque
        cd = ImageDraw.Draw(char_img)
        cd.text((3-cb[0], 3-cb[1]), ch, font=fnt, fill=0)   # black = cutout

        rot_deg = char_deg + (180 if flip else 0)
        rotated = char_img.rotate(-rot_deg, expand=True, resample=Image.BICUBIC,
                                  fillcolor=255)
        # composite: paste only where disc is opaque already
        rx, ry = rotated.size
        px, py = tx - rx//2, ty - ry//2
        region = alpha.crop((px, py, px+rx, py+ry))
        # multiply: keeps existing transparent pixels transparent
        from PIL import ImageChops
        merged = ImageChops.multiply(region, rotated)
        alpha.paste(merged, (px, py))

arc_r_text = int(R * 0.80)
arc_text("MODEL RAILROAD",       arc_r=arc_r_text, mid_deg=0,
         font_size=SCALE*11)
arc_text("CHEAP YELLOW DISPLAY", arc_r=arc_r_text, mid_deg=180,
         font_size=SCALE*10, flip=True)

# ── downscale 4× with antialiasing ───────────────────────────────────────────
result = alpha.resize((SIZE, SIZE), Image.LANCZOS)
a8 = list(result.tobytes())
assert len(a8) == SIZE * SIZE

# ── write C source ────────────────────────────────────────────────────────────
def chunks(lst, n):
    for i in range(0, len(lst), n): yield lst[i:i+n]

hex_lines = ",\n  ".join(
    ", ".join(f"0x{b:02X}" for b in row)
    for row in chunks(a8, 16)
)

OUT = os.path.join(os.path.dirname(__file__), "..", "src", "ui", "logo.c")
with open(OUT, "w") as f:
    f.write(f"""\
#include "logo.h"

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif
#ifndef LV_ATTRIBUTE_IMG_LOGO
#define LV_ATTRIBUTE_IMG_LOGO
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_LOGO uint8_t logo_map[] = {{
  {hex_lines}
}};

const lv_image_dsc_t logo = {{
  .header.magic  = LV_IMAGE_HEADER_MAGIC,
  .header.cf     = LV_COLOR_FORMAT_A8,
  .header.flags  = 0,
  .header.w      = {SIZE},
  .header.h      = {SIZE},
  .header.stride = {SIZE},
  .data_size     = {SIZE * SIZE},
  .data          = logo_map,
}};
""")
print(f"Written {OUT}  ({SIZE}x{SIZE})")

# ── preview: white icon on dark background ────────────────────────────────────
preview_path = os.path.join(os.path.dirname(__file__), "logo_preview.png")
bg  = Image.new("RGB", (SIZE, SIZE), (30, 30, 30))
ico = Image.new("RGB", (SIZE, SIZE), (255, 255, 255))
bg.paste(ico, mask=result)
bg.save(preview_path)
print(f"Preview:  {preview_path}")
