"""
Convert demo BMP frames from SD card into an animated GIF.

Usage:
    python tools/make_gif.py [frames_dir] [output.gif]

Defaults:
    frames_dir  = current directory
    output.gif  = demo.gif

Requires Pillow:
    pip install Pillow
"""

import sys
import glob
import os
from PIL import Image


def main():
    frames_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output     = sys.argv[2] if len(sys.argv) > 2 else "demo.gif"
    delay_ms   = int(sys.argv[3]) if len(sys.argv) > 3 else 1000

    pattern = os.path.join(frames_dir, "demo_*.bmp")
    paths   = sorted(glob.glob(pattern))

    if not paths:
        print(f"No demo_*.bmp files found in: {frames_dir}")
        sys.exit(1)

    print(f"Found {len(paths)} frame(s):")
    for p in paths:
        print(f"  {p}")

    frames = [Image.open(p).convert("RGB") for p in paths]

    frames[0].save(
        output,
        format="GIF",
        save_all=True,
        append_images=frames[1:],
        duration=delay_ms,
        loop=0,
        optimize=True,
    )

    size_kb = os.path.getsize(output) // 1024
    print(f"\nSaved: {output}  ({len(frames)} frames, {delay_ms}ms delay, {size_kb} KB)")


if __name__ == "__main__":
    main()
