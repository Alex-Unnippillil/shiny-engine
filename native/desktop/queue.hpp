// SPDX-License-Identifier: MIT
#pragma once
#include <QList>
#include <QString>
#include <QUrl>
namespace shiny::desktop {
struct Media { QUrl url; QString title; };
constexpr qsizetype MaxItems = 500;
// This edition accepts existing local regular files only. Network playback,
// remote playlist resolution and embedded credentials are deliberately absent.
Media localMedia(const QString& path);
bool matchesTitle(const QString& title, const QString& query);
QList<Media> readPlaylist(const QString& path);
void savePlaylist(const QString& path, const QList<Media>& items);
QString clockText(qint64 milliseconds);
}
