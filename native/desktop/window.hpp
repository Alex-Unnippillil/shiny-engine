// SPDX-License-Identifier: MIT
#pragma once
#include "queue.hpp"
#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoFrame>
class QAudioOutput;
class QVideoWidget;
class QListWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QSlider;
class QComboBox;
class QStackedWidget;
class QSettings;
namespace shiny::desktop {
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(bool ephemeral = false);
    bool addFiles(const QStringList& paths, bool autoplay = true);
    void replaceQueue(QList<Media> items);
    void playIndex(int index);
    void clearQueue();
    void removeSelected();
    void togglePlayback();
    void selectNext(int direction = 1);
    void setDark(bool enabled);
    void stop();
    void snapshot(const QString& path);
    int currentIndex() const { return current; }
    const QList<Media>& queue() const { return items; }
    QMediaPlayer* mediaPlayer() const { return player; }
    quint64 decodedFrames() const { return frames; }
    bool frameAvailable() const { return latest.isValid(); }
    QString lastError() const { return error; }
protected:
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void closeEvent(QCloseEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
private:
    void buildUi();
    void buildMenu();
    void refreshQueue();
    void refreshState();
    void refreshTracks();
    void openFiles();
    void openPlaylist();
    void exportPlaylist();
    void exportSnapshot();
    void showError(const QString& message);
    void updatePosition();
    void toggleFullscreen();
    QMediaPlayer* player = nullptr;
    QAudioOutput* audio = nullptr;
    QVideoWidget* video = nullptr;
    QStackedWidget* viewport = nullptr;
    QListWidget* list = nullptr;
    QLineEdit* filter = nullptr;
    QPushButton* play = nullptr;
    QPushButton* stopButton = nullptr;
    QPushButton* previous = nullptr;
    QPushButton* next = nullptr;
    QPushButton* capture = nullptr;
    QPushButton* mute = nullptr;
    QLabel* title = nullptr;
    QLabel* status = nullptr;
    QLabel* time = nullptr;
    QSlider* seek = nullptr;
    QSlider* volume = nullptr;
    QComboBox* rate = nullptr;
    QComboBox* repeat = nullptr;
    QComboBox* audioTracks = nullptr;
    QComboBox* subtitles = nullptr;
    QWidget* sidebar = nullptr;
    QSettings* settings = nullptr;
    QList<Media> items;
    QVideoFrame latest;
    int current = -1;
    quint64 frames = 0;
    quint64 generation = 0;
    bool closing = false;
    bool dark = true;
    QString error;
};
}
