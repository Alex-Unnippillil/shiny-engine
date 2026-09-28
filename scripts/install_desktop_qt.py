#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Use aqt's verified downloader with Qt 6.11's Windows repository naming.

Upstream aqt 3.3.0 issue #1007 documents the compiler-specific subdirectory.
Only this exact Windows kit's metadata path changes; hash verification, archive
selection and extraction remain aqt's implementation. No SDK code is patched.
"""
from importlib.metadata import version
import sys


def main() -> int:
    if version('aqtinstall') != '3.3.0':
        raise RuntimeError('Re-review the metadata adapter before changing aqt versions')
    from aqt.archives import QtArchives
    from aqt import main as aqt_main
    original = QtArchives._arch_ext

    def extension(self):
        if self.os_name == 'windows' and str(self.version) == '6.11.2' and self.arch == 'win64_msvc2022_64':
            return '_msvc2022_64'
        return original(self)

    QtArchives._arch_ext = extension
    return aqt_main()


if __name__ == '__main__':
    sys.exit(main())
