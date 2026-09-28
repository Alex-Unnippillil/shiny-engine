// SPDX-License-Identifier: MIT
#include "window.hpp"
#include <QApplication>
#include <QAction>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>
using namespace shiny::desktop;
class DesktopTests : public QObject {
    Q_OBJECT
    QString makeFile(const QString& folder, const QString& name) {
        const QString path = folder + '/' + name;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write("fixture") != 7) qFatal("Cannot create fixture");
        return path;
    }
private slots:
    void clockFormatting() {
        QCOMPARE(clockText(-10), QString("0:00"));
        QCOMPARE(clockText(65100), QString("1:05"));
        QCOMPARE(clockText(3600000), QString("1:00:00"));
        QCOMPARE(clockText(90061000), QString("25:01:01"));
    }
    void titleMatching() {
        QVERIFY(matchesTitle("Summer final.MP4", " FINAL  summer "));
        QVERIFY(matchesTitle("été.mov", "ÉTÉ"));
        QVERIFY(!matchesTitle("movie.mp4", "private-folder"));
        QVERIFY(matchesTitle("title", ""));
    }
    void localOnly() {
        QTemporaryDir d; QVERIFY(d.isValid());
        const auto file = makeFile(d.path(), "clip one.avi");
        QVERIFY(localMedia(file).url.isLocalFile());
        QCOMPARE(localMedia(file).title, QString("clip one.avi"));
        QVERIFY_EXCEPTION_THROWN(localMedia("https://example.test/video.mp4"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(localMedia("//server/media.mp4"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(localMedia("\\\\server\\media.mp4"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(localMedia("relative.mp4"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(localMedia(d.path()), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(localMedia(d.path() + "/missing"), std::runtime_error);
    }
    void playlistRoundTrip() {
        QTemporaryDir d;
        QList<Media> items{localMedia(makeFile(d.path(), "first clip.avi")), localMedia(makeFile(d.path(), "second-été.wav"))};
        const auto path = d.path() + "/list.shiny.json";
        savePlaylist(path, items);
        const auto loaded = readPlaylist(path);
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[1].url, items[1].url);
        savePlaylist(path, {items[1]});
        QCOMPARE(readPlaylist(path).size(), 1);
    }
    void playlistRejectsMalformedAndRemote() {
        QTemporaryDir d; const auto path = d.path() + "/bad.shiny.json";
        const QList<QByteArray> bad{"not json", "[]", "{\"schema\":2,\"files\":[]}",
            "{\"schema\":1,\"files\":[\"https://example.test/a.mp4\"]}",
            "{\"schema\":1,\"files\":[22]}", "{\"schema\":1,\"files\":[],\"command\":\"run\"}"};
        for (const auto& bytes : bad) {
            QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(bytes); f.close();
            QVERIFY_EXCEPTION_THROWN(readPlaylist(path), std::runtime_error);
        }
        QFile big(path); QVERIFY(big.open(QIODevice::WriteOnly)); big.resize(1024 * 1024 + 1); big.close();
        QVERIFY_EXCEPTION_THROWN(readPlaylist(path), std::runtime_error);
    }
    void limits() {
        QTemporaryDir d; const auto file = makeFile(d.path(), "clip.avi");
        QList<Media> tooMany;
        for (int i = 0; i < 501; ++i) tooMany.append(localMedia(file));
        QVERIFY_EXCEPTION_THROWN(savePlaylist(d.path() + "/list", tooMany), std::runtime_error);
        Window w(true); QVERIFY(!w.addFiles(QStringList(501, file), false)); QCOMPARE(w.queue().size(), 0);
    }
    void externalToolIsOptional() {
        Window w(true);
        auto* action = w.findChild<QAction*>("externalSwapper");
        QVERIFY(action);
#ifdef Q_OS_WIN
        QVERIFY(action->isEnabled());
#else
        QVERIFY(!action->isEnabled());
#endif
        QCOMPARE(w.mediaPlayer()->playbackState(), QMediaPlayer::StoppedState);
        QVERIFY(w.queue().isEmpty());
    }
    void idleUi() {
        Window w(true); w.show();
        QVERIFY(!w.findChild<QPushButton*>("play")->isEnabled());
        QVERIFY(!w.findChild<QPushButton*>("snapshot")->isEnabled());
        QVERIFY(!w.findChild<QSlider*>("seek")->isEnabled());
        QVERIFY(w.findChild<QPushButton*>("openFiles")->isEnabled());
        QVERIFY_EXCEPTION_THROWN(w.snapshot("unused.png"), std::runtime_error);
    }
    void filteredRemovalMapsToModel() {
        QTemporaryDir d;
        const auto first = makeFile(d.path(), "first.avi");
        const auto second = makeFile(d.path(), "second.avi");
        Window w(true); QVERIFY(w.addFiles({first, second}, false));
        auto* filter = w.findChild<QLineEdit*>("queueFilter");
        filter->setText("second");
        auto* list = w.findChild<QListWidget*>("queue");
        QCOMPARE(list->count(), 1); QCOMPARE(list->item(0)->data(Qt::UserRole).toInt(), 1);
        list->setCurrentRow(0); w.removeSelected();
        QCOMPARE(w.queue().size(), 1); QCOMPARE(w.queue()[0].title, QString("first.avi"));
        w.clearQueue(); QCOMPARE(w.queue().size(), 0); QCOMPARE(w.currentIndex(), -1);
        QCOMPARE(w.mediaPlayer()->playbackState(), QMediaPlayer::StoppedState);
    }
    void appendFailureIsAtomic() {
        QTemporaryDir d; const auto file = makeFile(d.path(), "safe.avi");
        Window w(true); QVERIFY(w.addFiles({file}, false));
        QVERIFY(!w.addFiles({file, d.path() + "/not-present.avi"}, false));
        QCOMPARE(w.queue().size(), 1); QCOMPARE(w.currentIndex(), -1);
    }
    void textInputDoesNotStartPlayback() {
        QTemporaryDir d; Window w(true); w.show();
        QVERIFY(w.addFiles({makeFile(d.path(), "sample.avi")}, false));
        auto* filter = w.findChild<QLineEdit*>("queueFilter"); filter->setFocus();
        QTest::keyClicks(filter, "a test");
        QCOMPARE(filter->text(), QString("a test"));
        QCOMPARE(w.mediaPlayer()->playbackState(), QMediaPlayer::StoppedState);
    }
    void compactControlsAndThemes() {
        Window w(true); w.show(); w.resize(760, 560); QTest::qWait(50);
        for (const auto* name : {"play", "stop", "snapshot", "openFiles"}) {
            auto* control = w.findChild<QPushButton*>(name); QVERIFY(control);
            QVERIFY(w.rect().contains(QRect(control->mapTo(&w, QPoint()), control->size())));
        }
        w.setDark(false); QVERIFY(w.styleSheet().isEmpty()); w.setDark(true); QVERIFY(!w.styleSheet().isEmpty());
    }
};
QTEST_MAIN(DesktopTests)
#include "test_desktop.moc"
