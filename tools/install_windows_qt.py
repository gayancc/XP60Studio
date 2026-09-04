"""Install the project Qt baseline from Qt's official, checksum-checked archives.

Run with aqtinstall==3.3.0 installed. That release does not yet map the
Windows MinGW split repository introduced by Qt 6.8; this narrow adapter
selects the published qt6_6112_mingw directory without disabling hashes.
"""

import sys
from pathlib import Path

import aqt
from aqt.metadata import QtRepoProperty


if __name__ == "__main__":
    original = QtRepoProperty.extension_for_arch
    QtRepoProperty.extension_for_arch = staticmethod(
        lambda arch, is_qt6: "mingw" if arch == "win64_mingw" and is_qt6 else original(arch, is_qt6)
    )
    sys.argv = [
        "aqt", "install-qt", "windows", "desktop", "6.11.2", "win64_mingw",
        "-O", str(Path(__file__).resolve().parents[1] / ".qt"),
        "--archives", "qtbase", "qtdeclarative", "qttools", "qtsvg",
    ]
    sys.exit(aqt.main())
