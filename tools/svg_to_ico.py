#!/usr/bin/env python3
"""Convert logo.svg to logo.ico for Windows EXE icon.
Uses Inkscape (if in PATH or standard locations) + Pillow. Requires: pip install pillow"""
import os
import subprocess
import sys
import tempfile

def find_inkscape():
    for path in [
        "inkscape",
        r"C:\Program Files\Inkscape\bin\inkscape.exe",
        r"C:\Program Files (x86)\Inkscape\bin\inkscape.exe",
    ]:
        if path == "inkscape":
            try:
                subprocess.run([path, "--version"], capture_output=True, check=True)
                return path
            except (subprocess.CalledProcessError, FileNotFoundError):
                continue
        if os.path.isfile(path):
            return path
    return None

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    svg_path = os.path.join(project_root, "resources", "icons", "logo.svg")
    ico_path = os.path.join(project_root, "resources", "icons", "logo.ico")

    if not os.path.exists(svg_path):
        print(f"Error: {svg_path} not found")
        return 1

    try:
        from PIL import Image
    except ImportError:
        print("Install: pip install pillow")
        return 1

    inkscape = find_inkscape()
    if not inkscape:
        print("Inkscape not found. Install Inkscape or use ImageMagick: magick -density 384 resources/icons/logo.svg -define icon:auto-resize resources/icons/logo.ico")
        return 1

    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        png_path = tmp.name
    try:
        subprocess.run(
            [inkscape, svg_path, "-w", "256", "-h", "256", "-o", png_path],
            capture_output=True,
            check=True,
        )
        img = Image.open(png_path).convert("RGBA")
    finally:
        os.unlink(png_path)
    sizes = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(ico_path, format="ICO", sizes=sizes)
    print(f"Created {ico_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
