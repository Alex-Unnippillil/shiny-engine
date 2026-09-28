// SPDX-License-Identifier: MIT
#include "queue.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <stdexcept>
namespace shiny::desktop {
Media localMedia(const QString& path) {
    if (path.contains(QChar::Null) || path.size() > 32768 || path.startsWith("//") || path.startsWith("\\\\"))
        throw std::runtime_error("Choose an existing local media file; network paths are not supported.");
    const QFileInfo file(path);
    if (!file.isAbsolute() || !file.isFile() || !file.isReadable())
        throw std::runtime_error("The selected local file is missing or unreadable.");
    const auto canonical = file.canonicalFilePath();
    if (canonical.isEmpty()) throw std::runtime_error("Cannot resolve the selected local file.");
    return {QUrl::fromLocalFile(canonical), file.fileName()};
}
bool matchesTitle(const QString& title, const QString& query) {
    const auto tokens = query.simplified().split(' ', Qt::SkipEmptyParts);
    for (const auto& token : tokens) if (!title.contains(token, Qt::CaseInsensitive)) return false;
    return true;
}
QList<Media> readPlaylist(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        throw std::runtime_error("Playlist is unreadable or exceeds the 1 MiB limit.");
    const auto bytes = file.read(1024 * 1024 + 1);
    if (bytes.size() > 1024 * 1024 || !file.atEnd())
        throw std::runtime_error("Playlist exceeds the 1 MiB limit.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    const auto root = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject() || root.size() != 2 ||
        !root.value("schema").isDouble() || root.value("schema").toDouble() != 1 || !root.value("files").isArray())
        throw std::runtime_error("This is not a Shiny Desktop version-1 playlist.");
    const auto files = root.value("files").toArray();
    if (files.size() > MaxItems) throw std::runtime_error("A playlist can contain at most 500 files.");
    QList<Media> items;
    for (const auto& value : files) {
        if (!value.isString()) throw std::runtime_error("Playlist entries must be local file paths.");
        items.append(localMedia(value.toString()));
    }
    return items; // All-or-nothing: never partially mutate the visible queue.
}
void savePlaylist(const QString& path, const QList<Media>& items) {
    if (items.size() > MaxItems) throw std::runtime_error("A playlist can contain at most 500 files.");
    QJsonArray files;
    for (const auto& item : items) {
        if (!item.url.isLocalFile()) throw std::runtime_error("Only local files may be saved in a playlist.");
        files.append(item.url.toLocalFile());
    }
    const auto data = QJsonDocument(QJsonObject{{"schema", 1}, {"files", files}}).toJson();
    if (data.size() > 1024 * 1024) throw std::runtime_error("Playlist exceeds the 1 MiB limit.");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        throw std::runtime_error("Could not save the playlist atomically.");
}
QString clockText(qint64 ms) {
    const auto seconds = qMax(qint64(0), ms) / 1000;
    const auto hours = seconds / 3600;
    const auto minutes = seconds / 60 % 60;
    return hours ? QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'))
                 : QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}
}
