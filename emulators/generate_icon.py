import os

from PIL import Image, ImageDraw

width = 30
height = 30

img = Image.new('L', (width, height), 0)
draw = ImageDraw.Draw(img)

# Outline parameters
line_width = 2
color = 255

# Main body
draw.rounded_rectangle([4, 4, 25, 22], radius=4, outline=color, width=line_width)

# Roof
draw.rounded_rectangle([7, 2, 22, 4], radius=1, outline=color, width=line_width)

# Windows
draw.rounded_rectangle([7, 8, 13, 14], radius=2, outline=color, width=line_width)
draw.rounded_rectangle([16, 8, 22, 14], radius=2, outline=color, width=line_width)

# Headlights
draw.ellipse([7, 17, 10, 20], outline=color, width=line_width)
draw.ellipse([19, 17, 22, 20], outline=color, width=line_width)

# Bumper
draw.line([8, 22, 8, 24], fill=color, width=line_width)
draw.line([21, 22, 21, 24], fill=color, width=line_width)
draw.line([6, 24, 23, 24], fill=color, width=line_width)

# Tracks
draw.line([10, 24, 6, 29], fill=color, width=line_width)
draw.line([19, 24, 23, 29], fill=color, width=line_width)

pixels = list(img.getdata())

# Generate ARGB8888 C array
# In LVGL 9, ARGB8888 format is [B, G, R, A] in memory (little endian)
c_array = "const uint8_t train_icon_map[] = {\n"
for i in range(0, len(pixels), 30):
    row = pixels[i:i+30]
    # For a white train, B=255, G=255, R=255, A=pixel
    c_array += "    " + ", ".join([f"0xff, 0xff, 0xff, 0x{p:02x}" for p in row]) + ",\n"
c_array += "};\n"

c_code = f"""#include "lvgl.h"

{c_array}

const lv_image_dsc_t train_icon = {{
  .header = {{
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_ARGB8888,
    .flags = 0,
    .w = {width},
    .h = {height},
    .stride = {width * 4},
  }},
  .data_size = sizeof(train_icon_map),
  .data = train_icon_map,
}};
"""

with open("c:/Users/Caleb Smith/Documents/GitHub/EX-T3-WiFi/src/train_icon.c", "w") as f:
    f.write(c_code)

print("Icon generated successfully.")
