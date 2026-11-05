# Application icon

The supplied `a-premium-3d-glossy-desktop-application-icon--a-ro.tiff` is the
source artwork. Generated files preserve its composition and background:

- `xp60studio.png`: Qt application/window icon, embedded as a resource.
- `xp60studio.ico`: Windows executable icon, 16 through 256 pixels.
- `xp60studio.icns`: macOS bundle icon.

Regenerate with `python tools/generate_app_icons.py` using Python with Pillow.
The original is CMYK without an embedded profile; screen formats use RGB.
Generated files are checked in, so building the app does not require Pillow.
