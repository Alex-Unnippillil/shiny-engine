// SPDX-License-Identifier: MIT
#include "window.hpp"
#include <QAction>
#include <QApplication>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaMetaData>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoSink>
#include <QVideoWidget>
#include <stdexcept>
namespace shiny::desktop {
namespace {
QLabel* label(const QString& text, const char* name = nullptr) {
    auto* result = new QLabel(text);
    result->setTextFormat(Qt::PlainText);
    if (name) result->setObjectName(name);
    return result;
}
QPushButton* button(const QString& text, const char* name, const QString& help) {
    auto* result = new QPushButton(text);
    result->setObjectName(name);
    result->setAccessibleName(text);
    result->setToolTip(help);
    result->setMinimumHeight(34);
    return result;
}
}
Window::Window(bool ephemeral) {
    setWindowTitle("Shiny Desktop — local media");
    setObjectName("shinyDesktop");
    resize(1180, 760);
    setMinimumSize(720, 520);
    setAcceptDrops(true);
    player = new QMediaPlayer(this);
    audio = new QAudioOutput(this);
    player->setAudioOutput(audio);
    if (!ephemeral) settings = new QSettings(this);
    buildUi();
    buildMenu();
    audio->setVolume(static_cast<float>(settings ? qBound(0.0, settings->value("volume", 0.65).toDouble(), 1.0) : 0.65));
    volume->setValue(qRound(audio->volume() * 100));
    setDark(settings ? settings->value("dark", true).toBool() : true);
    connect(filter, &QLineEdit::textChanged, this, &Window::refreshQueue);
    connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) { playIndex(item->data(Qt::UserRole).toInt()); });
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this] { refreshState(); });
    connect(player, &QMediaPlayer::durationChanged, this, [this] { updatePosition(); refreshState(); });
    connect(player, &QMediaPlayer::positionChanged, this, [this] { updatePosition(); });
    connect(player, &QMediaPlayer::seekableChanged, this, [this] { refreshState(); });
    connect(player, &QMediaPlayer::hasVideoChanged, this, [this] { refreshState(); });
    connect(player, &QMediaPlayer::tracksChanged, this, &Window::refreshTracks);
    connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString&) {
        latest = {}; capture->setEnabled(false);
        showError("Playback unavailable. Check the file, format and installed media codecs. Stop or choose another file.");
    });
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus state) {
        refreshState();
        if (state == QMediaPlayer::EndOfMedia) {
            const auto source = player->source();
            const auto session = generation;
            QTimer::singleShot(0, this, [this, source, session] {
                if (closing || session != generation || player->source() != source || player->mediaStatus() != QMediaPlayer::EndOfMedia) return;
                if (repeat->currentIndex() == 1) { player->setPosition(0); player->play(); }
                else if (current + 1 < items.size() || repeat->currentIndex() == 2) selectNext();
            });
        }
    });
    connect(video->videoSink(), &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
        if (!closing && frame.isValid() && player->playbackState() != QMediaPlayer::StoppedState) {
            latest = frame; ++frames; capture->setEnabled(true);
        }
    });
    refreshTracks();
    refreshState();
}
void Window::buildUi() {
    auto* central = new QWidget;
    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(20, 16, 20, 16);
    outer->setSpacing(12);
    auto* header = new QHBoxLayout;
    auto* brand = label("SHINY", "brand");
    auto* caption = label("DESKTOP  /  LOCAL MEDIA", "caption");
    header->addWidget(brand); header->addSpacing(12); header->addWidget(caption); header->addStretch();
    auto* open = button("Open files", "openFiles", "Open local video or audio (Ctrl/Cmd+O)");
    connect(open, &QPushButton::clicked, this, &Window::openFiles);
    header->addWidget(open);
    outer->addLayout(header);
    title = label("Your media. Your device.", "mediaTitle");
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    outer->addWidget(title);
    auto* splitter = new QSplitter;
    viewport = new QStackedWidget;
    viewport->setMinimumWidth(320);
    auto* empty = new QWidget;
    auto* welcome = new QVBoxLayout(empty);
    welcome->addStretch();
    auto* heading = label("Make room for the picture.", "welcomeTitle");
    heading->setAlignment(Qt::AlignCenter);
    auto* detail = label("Drop local video or audio here.\nNo account, cloud upload or automatic file history.", "welcomeDetail");
    detail->setAlignment(Qt::AlignCenter); detail->setWordWrap(true);
    auto* start = button("Choose media", "welcomeOpen", "Open local files");
    connect(start, &QPushButton::clicked, this, &Window::openFiles);
    welcome->addWidget(heading); welcome->addSpacing(10); welcome->addWidget(detail);
    welcome->addSpacing(16); welcome->addWidget(start, 0, Qt::AlignHCenter); welcome->addStretch();
    video = new QVideoWidget;
    video->setObjectName("video");
    video->setAccessibleName("Video playback surface");
    video->setAspectRatioMode(Qt::KeepAspectRatio);
    video->setFocusPolicy(Qt::StrongFocus);
    video->installEventFilter(this);
    player->setVideoOutput(video);
    viewport->addWidget(empty); viewport->addWidget(video);
    splitter->addWidget(viewport);
    sidebar = new QWidget;
    sidebar->setObjectName("sidebar"); sidebar->setMinimumWidth(220); sidebar->setMaximumWidth(360);
    auto* queueLayout = new QVBoxLayout(sidebar);
    queueLayout->setContentsMargins(14, 12, 14, 12);
    queueLayout->addWidget(label("PLAY QUEUE", "section"));
    filter = new QLineEdit;
    filter->setObjectName("queueFilter"); filter->setPlaceholderText("Search titles…"); filter->setClearButtonEnabled(true);
    filter->setAccessibleName("Search queue titles"); filter->setMaxLength(256);
    queueLayout->addWidget(filter);
    list = new QListWidget;
    list->setObjectName("queue"); list->setAccessibleName("Play queue");
    list->setAlternatingRowColors(false); list->installEventFilter(this);
    queueLayout->addWidget(list);
    auto* queueTools = new QHBoxLayout;
    auto* remove = button("Remove", "removeItem", "Remove highlighted item, not the filtered row number");
    auto* clear = button("Clear", "clearQueue", "Stop playback and clear the in-memory queue");
    connect(remove, &QPushButton::clicked, this, &Window::removeSelected);
    connect(clear, &QPushButton::clicked, this, &Window::clearQueue);
    queueTools->addWidget(remove); queueTools->addWidget(clear);
    queueLayout->addLayout(queueTools);
    auto* note = label("Queue is temporary.\nSave a playlist only when you choose.", "queueNote");
    note->setWordWrap(true); queueLayout->addWidget(note);
    splitter->addWidget(sidebar); splitter->setStretchFactor(0, 1);
    splitter->setSizes({850, 270}); outer->addWidget(splitter, 1);
    auto* timeline = new QHBoxLayout;
    seek = new QSlider(Qt::Horizontal);
    seek->setObjectName("seek"); seek->setRange(0, 10000); seek->setAccessibleName("Seek position");
    connect(seek, &QSlider::sliderReleased, this, [this] {
        if (player->isSeekable()) player->setPosition(player->duration() * seek->value() / 10000);
    });
    connect(seek, &QSlider::actionTriggered, this, [this](int action) {
        // Keyboard/page-step actions do not emit sliderReleased. The thumb's
        // position is updated before valueChanged and must be read directly.
        if (action != QAbstractSlider::SliderMove && player->isSeekable())
            player->setPosition(player->duration() * seek->sliderPosition() / 10000);
    });
    time = label("0:00 / 0:00", "time"); time->setMinimumWidth(110);
    timeline->addWidget(seek, 1); timeline->addWidget(time); outer->addLayout(timeline);
    auto* transport = new QHBoxLayout;
    previous = button("Previous", "previous", "Previous queue item");
    play = button("Play", "play", "Play or pause; Space when video has focus");
    stopButton = button("Stop", "stop", "Stop playback immediately");
    next = button("Next", "next", "Next queue item");
    capture = button("Snapshot", "snapshot", "Explicitly save the current decoded frame as PNG");
    connect(previous, &QPushButton::clicked, this, [this] { selectNext(-1); });
    connect(play, &QPushButton::clicked, this, &Window::togglePlayback);
    connect(stopButton, &QPushButton::clicked, this, &Window::stop);
    connect(next, &QPushButton::clicked, this, [this] { selectNext(); });
    connect(capture, &QPushButton::clicked, this, &Window::exportSnapshot);
    for (auto* b : {previous, play, stopButton, next, capture}) transport->addWidget(b);
    transport->addStretch();
    mute = button("Mute", "mute", "Mute or unmute playback audio");
    connect(mute, &QPushButton::clicked, this, [this] {
        audio->setMuted(!audio->isMuted()); mute->setText(audio->isMuted() ? "Unmute" : "Mute");
    });
    volume = new QSlider(Qt::Horizontal); volume->setRange(0, 100); volume->setMaximumWidth(110);
    volume->setObjectName("volume"); volume->setAccessibleName("Volume");
    connect(volume, &QSlider::valueChanged, this, [this](int value) { audio->setVolume(value / 100.0f); });
    transport->addWidget(mute); transport->addWidget(volume); outer->addLayout(transport);
    auto* options = new QHBoxLayout;
    rate = new QComboBox; rate->setObjectName("rate"); rate->setAccessibleName("Playback speed");
    for (double value : {0.5, 0.75, 1.0, 1.25, 1.5, 2.0}) rate->addItem(QString::number(value) + "×", value);
    rate->setCurrentIndex(2);
    connect(rate, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { player->setPlaybackRate(rate->currentData().toDouble()); });
    repeat = new QComboBox; repeat->setObjectName("repeat"); repeat->setAccessibleName("Repeat mode");
    repeat->addItems({"No repeat", "Repeat one", "Repeat queue"});
    audioTracks = new QComboBox; audioTracks->setObjectName("audioTracks"); audioTracks->setAccessibleName("Audio track");
    subtitles = new QComboBox; subtitles->setObjectName("subtitles"); subtitles->setAccessibleName("Embedded subtitle track");
    connect(audioTracks, qOverload<int>(&QComboBox::activated), this, [this] { player->setActiveAudioTrack(audioTracks->currentData().toInt()); });
    connect(subtitles, qOverload<int>(&QComboBox::activated), this, [this] { player->setActiveSubtitleTrack(subtitles->currentData().toInt()); });
    options->addWidget(label("Speed")); options->addWidget(rate); options->addWidget(repeat);
    options->addWidget(audioTracks, 1); options->addWidget(subtitles, 1); outer->addLayout(options);
    status = label("Ready · Local playback · Qt edition", "status"); status->setWordWrap(true);
    outer->addWidget(status); setCentralWidget(central);
}
void Window::buildMenu() {
    auto* media = menuBar()->addMenu("&Media");
    auto* open = media->addAction("Open files…"); open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &Window::openFiles);
    connect(media->addAction("Open Shiny playlist…"), &QAction::triggered, this, &Window::openPlaylist);
    connect(media->addAction("Save Shiny playlist…"), &QAction::triggered, this, &Window::exportPlaylist);
    media->addSeparator();
    auto* quit = media->addAction("Quit"); quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto* playback = menuBar()->addMenu("&Playback");
    connect(playback->addAction("Play / pause"), &QAction::triggered, this, &Window::togglePlayback);
    auto* stopAction = playback->addAction("Stop"); stopAction->setShortcut(QKeySequence("Ctrl+."));
    connect(stopAction, &QAction::triggered, this, &Window::stop);
    auto* view = menuBar()->addMenu("&View");
    auto* queueAction = view->addAction("Show queue"); queueAction->setCheckable(true); queueAction->setChecked(true);
    connect(queueAction, &QAction::toggled, this, [this](bool visible) {
        if (!visible && sidebar->isAncestorOf(QApplication::focusWidget())) video->setFocus();
        sidebar->setVisible(visible);
    });
    auto* find = view->addAction("Search queue"); find->setShortcut(QKeySequence::Find);
    connect(find, &QAction::triggered, this, [this, queueAction] { queueAction->setChecked(true); filter->setFocus(); });
    auto* fullscreen = view->addAction("Fullscreen"); fullscreen->setShortcut(QKeySequence("F11"));
    connect(fullscreen, &QAction::triggered, this, &Window::toggleFullscreen);
    auto* theme = view->addAction("Dark theme (off uses system palette)"); theme->setCheckable(true);
    theme->setChecked(settings ? settings->value("dark", true).toBool() : true);
    connect(theme, &QAction::toggled, this, &Window::setDark);
    auto* help = menuBar()->addMenu("&Help");
    connect(help->addAction("About / capabilities"), &QAction::triggered, this, [this] {
        QMessageBox::about(this, "Shiny Desktop " SHINY_DESKTOP_VERSION,
            "Native C++ / Qt local-media edition.\n\n"
            "Local video and audio, searchable queue, explicit playlists, playback speed, embedded tracks and snapshots.\n\n"
            "This edition does not load enhancement DLLs, neural models or capture screens. "
            "The existing Windows VLC player retains those separate tools. "
            "No DLSS, HDR fidelity, physical-GPU speed or universal-codec claim is made.\n\n"
            "Only volume and theme are saved automatically. Explicit playlists contain local paths. "
            "No telemetry, account or updater.\n\n"
            "Qt is dynamically linked under LGPLv3; see the included notices and source/build instructions. "
            "This prerelease is not certificate-signed or Apple-notarized.");
    });
    connect(help->addAction("Keyboard help"), &QAction::triggered, this, [this] {
        QMessageBox::information(this, "Keyboard controls", "Ctrl/Cmd+O: Open files\nCtrl/Cmd+F: Search queue\nCtrl/Cmd+.: Stop\nF11: Fullscreen\nEsc: Leave fullscreen\n\n"
            "Video focus: Space play/pause; Left/Right seek 5 seconds\nQueue focus: Enter play selected; Delete remove selected\nNative text-field and slider shortcuts are preserved.");
    });
}
void Window::setDark(bool enabled) {
    dark = enabled;
    if (!enabled) { setStyleSheet({}); return; }
    setStyleSheet(R"(
        QMainWindow, QWidget { background: #101621; color: #e5edf7; font-size: 13px; }
        QLabel#brand { font-size: 27px; font-weight: 700; letter-spacing: 4px; color: #81e3cc; }
        QLabel#caption, QLabel#queueNote { color: #a4b5ce; font-size: 11px; }
        QLabel#mediaTitle { font-size: 20px; font-weight: 600; padding: 4px 0; }
        QLabel#welcomeTitle { font-size: 25px; font-weight: 600; }
        QLabel#welcomeDetail { color: #a4b5ce; line-height: 1.4; }
        QWidget#sidebar { background: #182232; border-radius: 12px; }
        QLabel#section { font-weight: 600; letter-spacing: 2px; }
        QLabel#status { color: #a4b5ce; padding: 3px; }
        QPushButton { background: #223147; border: 1px solid #36465c; border-radius: 7px; padding: 5px 11px; }
        QPushButton:hover { background: #2d425e; }
        QPushButton:pressed { background: #385270; }
        QPushButton:disabled { color: #7b879a; background: #19222f; }
        QPushButton:focus, QLineEdit:focus, QComboBox:focus { border: 2px solid #81e3cc; }
        QPushButton#play, QPushButton#openFiles, QPushButton#welcomeOpen { background: #81e3cc; color: #101621; font-weight: 600; }
        QPushButton#play:disabled { background: #284b48; color: #8eaba9; }
        QLineEdit, QComboBox { border: 1px solid #36465c; border-radius: 6px; padding: 7px; background: #111b29; }
        QListWidget { border: none; background: #182232; outline: 0; }
        QListWidget::item { padding: 13px 7px; border-radius: 6px; margin: 2px 0; }
        QListWidget::item:selected { background: #2a4754; color: #c9fff1; }
        QSlider::groove:horizontal { height: 5px; background: #30425a; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #81e3cc; border-radius: 2px; }
        QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #c9fff1; border-radius: 7px; }
        QMenu::item:selected { background: #2a4754; }
        QSplitter::handle { background: #101621; width: 10px; }
    )");
}
bool Window::addFiles(const QStringList& paths, bool autoplay) {
    try {
        if (paths.size() + items.size() > MaxItems) throw std::runtime_error("Queue limit: 500 files. Clear or remove items first.");
        QList<Media> added;
        for (const auto& path : paths) added.append(localMedia(path));
        const auto first = int(items.size());
        items.append(added); error.clear(); refreshQueue(); refreshState();
        if (autoplay && !added.isEmpty()) playIndex(first);
        return true;
    } catch (const std::exception& e) { showError(QString::fromUtf8(e.what())); return false; }
}
void Window::replaceQueue(QList<Media> replacement) {
    if (replacement.size() > MaxItems) throw std::runtime_error("Queue limit: 500 files.");
    for (const auto& item : replacement) (void)localMedia(item.url.toLocalFile());
    clearQueue(); items = std::move(replacement); refreshQueue(); refreshState();
}
void Window::refreshQueue() {
    const int highlighted = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toInt() : current;
    const QSignalBlocker blocked(list); list->clear();
    for (int i = 0; i < items.size(); ++i) {
        if (!matchesTitle(items[i].title, filter->text())) continue;
        auto* item = new QListWidgetItem((i == current ? "▶  " : "") + items[i].title, list);
        item->setData(Qt::UserRole, i); item->setToolTip(items[i].title);
        if (i == highlighted) list->setCurrentItem(item);
    }
    if (list->currentRow() < 0 && list->count()) list->setCurrentRow(0);
}
void Window::playIndex(int index) {
    if (index < 0 || index >= items.size()) return;
    try {
        (void)localMedia(items[index].url.toLocalFile());
        ++generation; player->stop(); latest = {}; error.clear(); current = index;
        player->setSource(items[index].url); player->play();
        title->setText(items[index].title); refreshQueue(); refreshState();
    } catch (const std::exception& e) { showError(QString::fromUtf8(e.what())); }
}
void Window::togglePlayback() {
    if (player->playbackState() == QMediaPlayer::PlayingState) player->pause();
    else if (current >= 0 && !player->source().isEmpty()) player->play();
    else if (list->currentItem()) playIndex(list->currentItem()->data(Qt::UserRole).toInt());
    else if (!items.isEmpty()) playIndex(0);
}
void Window::stop() { ++generation; player->stop(); latest = {}; capture->setEnabled(false); refreshState(); }
void Window::clearQueue() {
    stop(); player->setSource({}); items.clear(); current = -1; latest = {}; error.clear(); filter->clear();
    title->setText("Your media. Your device."); refreshQueue(); refreshTracks(); refreshState();
}
void Window::removeSelected() {
    auto* selected = list->currentItem(); if (!selected) return;
    const int index = selected->data(Qt::UserRole).toInt();
    if (index < 0 || index >= items.size()) return;
    if (index == current) { stop(); player->setSource({}); current = -1; title->setText("Choose a queued item"); }
    else if (index < current) --current;
    items.removeAt(index); refreshQueue(); refreshState();
}
void Window::selectNext(int direction) {
    if (items.isEmpty()) return;
    int index = current < 0 ? 0 : current + direction;
    if (repeat->currentIndex() == 2) index = (index + int(items.size())) % int(items.size());
    if (index >= 0 && index < items.size()) playIndex(index);
}
void Window::refreshState() {
    const bool loaded = !player->source().isEmpty();
    play->setEnabled(!items.isEmpty()); stopButton->setEnabled(loaded);
    previous->setEnabled(!items.isEmpty()); next->setEnabled(!items.isEmpty());
    seek->setEnabled(player->isSeekable() && player->duration() > 0);
    capture->setEnabled(latest.isValid());
    play->setText(player->playbackState() == QMediaPlayer::PlayingState ? "Pause" : "Play");
    viewport->setCurrentIndex(loaded ? 1 : 0);
    QString text = "Ready";
    if (player->mediaStatus() == QMediaPlayer::LoadingMedia) text = "Loading";
    else if (player->playbackState() == QMediaPlayer::PlayingState) text = "Playing";
    else if (player->playbackState() == QMediaPlayer::PausedState) text = "Paused";
    else if (loaded) text = "Stopped";
    status->setText(error.isEmpty() ? text + " · Local playback · Qt edition" : error);
}
void Window::refreshTracks() {
    const QSignalBlocker a(audioTracks), s(subtitles);
    audioTracks->clear(); subtitles->clear(); subtitles->addItem("Subtitles off", -1);
    const auto audioList = player->audioTracks();
    for (int i = 0; i < audioList.size(); ++i) audioTracks->addItem(QString("Audio %1").arg(i + 1), i);
    if (audioList.isEmpty()) audioTracks->addItem("No audio track", -1);
    const auto subList = player->subtitleTracks();
    for (int i = 0; i < subList.size(); ++i) subtitles->addItem(QString("Subtitle %1").arg(i + 1), i);
    audioTracks->setCurrentIndex(qMax(0, audioTracks->findData(player->activeAudioTrack())));
    subtitles->setCurrentIndex(qMax(0, subtitles->findData(player->activeSubtitleTrack())));
    audioTracks->setEnabled(!audioList.isEmpty()); subtitles->setEnabled(!subList.isEmpty());
}
void Window::updatePosition() {
    time->setText(clockText(player->position()) + " / " + clockText(player->duration()));
    if (!seek->isSliderDown()) seek->setValue(player->duration() > 0 ? int(player->position() * 10000 / player->duration()) : 0);
}
void Window::showError(const QString& message) { error = message; status->setText(message); }
void Window::openFiles() {
    const auto files = QFileDialog::getOpenFileNames(this, "Open local media", {}, "Media (*.mp4 *.mkv *.mov *.webm *.avi *.mp3 *.m4a *.flac *.wav *.ogg);;All files (*)");
    if (!files.isEmpty()) addFiles(files);
}
void Window::openPlaylist() {
    const auto path = QFileDialog::getOpenFileName(this, "Open Shiny playlist", {}, "Shiny playlist (*.shiny.json)");
    if (path.isEmpty()) return;
    try { replaceQueue(readPlaylist(path)); status->setText("Playlist loaded. Choose Play to begin."); }
    catch (const std::exception& e) { showError(QString::fromUtf8(e.what())); }
}
void Window::exportPlaylist() {
    const auto path = QFileDialog::getSaveFileName(this, "Save playlist (contains local paths)", "playlist.shiny.json", "Shiny playlist (*.shiny.json)");
    if (path.isEmpty()) return;
    try { savePlaylist(path, items); status->setText("Playlist saved locally. It contains absolute media paths."); }
    catch (const std::exception& e) { showError(QString::fromUtf8(e.what())); }
}
void Window::snapshot(const QString& path) {
    if (!latest.isValid()) throw std::runtime_error("No decoded frame is available.");
    const auto image = latest.toImage();
    if (image.isNull()) throw std::runtime_error("This frame cannot be exported.");
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot create the snapshot.");
    QImageWriter writer(&output, "png");
    if (!writer.write(image) || !output.commit()) throw std::runtime_error("Could not save the snapshot atomically.");
}
void Window::exportSnapshot() {
    if (!latest.isValid()) return;
    const auto path = QFileDialog::getSaveFileName(this, "Save decoded frame", "shiny-frame.png", "PNG image (*.png)");
    if (path.isEmpty()) return;
    try { snapshot(path); status->setText("Decoded frame saved. Display scaling and subtitle overlays are not part of this export."); }
    catch (const std::exception& e) { showError(QString::fromUtf8(e.what())); }
}
void Window::toggleFullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }
bool Window::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && isFullScreen()) { showNormal(); return true; }
        if (object == list && key->key() == Qt::Key_Delete) { removeSelected(); return true; }
        if (object == video && key->modifiers() == Qt::NoModifier) {
            if (key->key() == Qt::Key_Space) { togglePlayback(); return true; }
            if ((key->key() == Qt::Key_Left || key->key() == Qt::Key_Right) && player->isSeekable()) {
                player->setPosition(qBound(qint64(0), player->position() + (key->key() == Qt::Key_Left ? -5000 : 5000), player->duration())); return true;
            }
        }
    }
    return QMainWindow::eventFilter(object, event);
}
void Window::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) return;
    const auto urls = event->mimeData()->urls();
    if (urls.size() > MaxItems) return;
    for (const auto& url : urls) if (!url.isLocalFile()) return;
    event->acceptProposedAction();
}
void Window::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) { showError("Only local media files can be dropped here."); return; }
        paths.append(url.toLocalFile());
    }
    if (addFiles(paths)) event->acceptProposedAction();
}
void Window::closeEvent(QCloseEvent* event) {
    closing = true; player->stop(); player->setSource({}); latest = {};
    if (settings) { settings->setValue("volume", audio->volume()); settings->setValue("dark", dark); settings->sync(); }
    event->accept();
}
}
