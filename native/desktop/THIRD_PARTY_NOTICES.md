# Shiny Desktop third-party notices

The original application code is MIT licensed (repository LICENSE). It dynamically links Qt Widgets, Qt Core/Gui/Network, Qt Multimedia and MultimediaWidgets; these modules are used under GNU LGPL version 3, not under a proprietary Qt license. Qt Test is a build/test dependency and is not packaged as an application runtime. The app does not restrict replacement/relinking or reverse engineering for debugging changes to LGPL components. Full application source and CMake build instructions are in the repository and source release.

Qt: Copyright The Qt Company Ltd. and other contributors. Qt's third-party components have their own terms. Windows/macOS packages use official Qt 6.11.2 shared binaries and their FFmpeg media backend. FFmpeg: Copyright the FFmpeg developers; LGPL-2.1-or-later and component-specific permissive licenses in Qt's distributed configuration. No separately downloaded GPL codec pack is bundled. Linux uses OS-managed Qt and FFmpeg/GStreamer packages under their distribution's copyright files; dependencies are not copied into the DEB.

Corresponding Qt source and license texts:
- https://download.qt.io/official_releases/qt/6.11/6.11.2/submodules/
- https://code.qt.io/cgit/qt/qtbase.git/tree/LICENSES?h=v6.11.2
- https://code.qt.io/cgit/qt/qtmultimedia.git/tree/LICENSES?h=v6.11.2
- https://code.qt.io/cgit/qt/qtmultimedia.git/tree/src/3rdparty/ffmpeg?h=v6.11.2
- https://doc.qt.io/qt-6/licenses-used-in-qt.html
- https://www.gnu.org/licenses/lgpl-3.0.html
- https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

Distribution must retain license/copyright notices and provide the corresponding source access required by the applicable licenses. Rebuilding or distributing a different codec configuration requires a fresh license and patent review; repository MIT licensing does not relicense Qt, FFmpeg, codecs, media or vendor models. No NVIDIA model/runtime, VideoLAN runtime, screenshots from third-party films or fonts are bundled by this desktop target.

Windows: Microsoft compiler runtime deployment is governed by Microsoft's redistributable license. macOS system frameworks and Linux system libraries remain subject to platform licenses. This file is a dependency summary, not a claim of a complete third-party compliance audit or full SBOM.
