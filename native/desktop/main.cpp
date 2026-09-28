// SPDX-License-Identifier: MIT
#include "window.hpp"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScreen>
#include <QSysInfo>
#include <QTimer>
#include <iostream>
using shiny::desktop::Window;
namespace {
int smoke(QApplication& app, const QString& fixture, const QString& destination) {
    QDir out(destination);
    if (!QDir().mkpath(destination)) return 2;
    for (const auto* file : {"playback.json", "desktop.png", "decoded-frame.png"})
        if (QFileInfo::exists(out.filePath(file))) return 2;
    Window window(true);
    window.show();
    if (!window.addFiles({fixture})) return 2;
    QElapsedTimer elapsed; elapsed.start();
    QTimer poll;
    int phase = 0;
    qint64 pausedAt = 0;
    bool sought = false;
    int result = 2;
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        const auto finish = [&](bool passed, const QString& reason) {
            QJsonObject report{{"schema", 1}, {"version", SHINY_DESKTOP_VERSION}, {"passed", passed},
                {"reason", reason}, {"deliveredVideoFrames", double(window.decodedFrames())},
                {"pauseVerified", phase >= 2}, {"seekVerified", sought}, {"stopVerified", passed},
                {"qt", QT_VERSION_STR}, {"architecture", QSysInfo::currentCpuArchitecture()},
                {"dlss", false}, {"physicalGpuCertified", false}, {"scope", "Synthetic local media; actual native playback and frame export. No physical audio-device or perceptual-quality certification."}};
            QSaveFile file(out.filePath("playback.json"));
            const auto bytes = QJsonDocument(report).toJson();
            const bool saved = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
            result = passed && saved ? 0 : 2;
            poll.stop(); window.close(); app.quit();
        };
        if (elapsed.elapsed() > 25000 || !window.lastError().isEmpty()) { finish(false, "playback-failed-or-timed-out"); return; }
        auto* player = window.mediaPlayer();
        if (phase == 0 && window.decodedFrames() >= 5 && player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause(); pausedAt = elapsed.elapsed(); phase = 1;
        } else if (phase == 1 && elapsed.elapsed() - pausedAt > 150 && player->playbackState() == QMediaPlayer::PausedState) {
            if (!player->isSeekable()) { finish(false, "local-fixture-not-seekable"); return; }
            player->setPosition(3000); player->play(); phase = 2;
        } else if (phase == 2 && window.frameAvailable() && player->position() >= 3000 && window.decodedFrames() >= 8) {
            sought = true;
            try {
                window.snapshot(out.filePath("decoded-frame.png"));
                if (!window.screen()->grabWindow(window.winId()).save(out.filePath("desktop.png"))) { finish(false, "window-capture-failed"); return; }
                window.stop(); phase = 3; pausedAt = elapsed.elapsed();
            } catch (...) { finish(false, "snapshot-export-failed"); }
        } else if (phase == 3 && elapsed.elapsed() - pausedAt > 150) {
            finish(player->playbackState() == QMediaPlayer::StoppedState, "native-play-pause-seek-snapshot-stop");
        }
    });
    poll.start(50); app.exec(); return result;
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ShinyDesktop");
    QCoreApplication::setOrganizationName("ShinyPlayer");
    QCoreApplication::setApplicationVersion(SHINY_DESKTOP_VERSION);
    const auto args = app.arguments();
    if (args.size() == 2 && args[1] == "--version") { std::cout << SHINY_DESKTOP_VERSION << '\n'; return 0; }
    if (args.size() == 4 && args[1] == "--smoke") return smoke(app, args[2], args[3]);
    for (int i = 1; i < args.size(); ++i) if (args[i].startsWith("--")) return 2;
    Window window;
    window.show();
    if (args.size() > 1) window.addFiles(args.mid(1));
    return app.exec();
}
