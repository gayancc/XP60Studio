"""Convert the supplied artwork to app icon formats (requires Pillow)."""
from pathlib import Path
from PIL import Image

assets = Path(__file__).resolve().parents[1] / "docs" / "assets"
source = assets / "a-premium-3d-glossy-desktop-application-icon--a-ro.tiff"

# The supplied TIFF is CMYK without an embedded color profile. Convert to RGB
# for screen formats, preserving the full artwork and its original background.
with Image.open(source) as original:
    icon = original.convert("RGB")
    icon.save(assets / "xp60studio.png")
    icon.save(assets / "xp60studio.ico", sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
    icon.save(assets / "xp60studio.icns")
