#pragma once

#include <QDir>
#include <QFile>
#include <memory>

namespace Utils {

inline std::unique_ptr<QFile> createPrivateLogFile(const QString& directory,
                                                   qint64 epochSeconds,
                                                   qint64 processId)
{
    const QString fileName = QStringLiteral("Moonlight-%1-%2.log")
            .arg(epochSeconds)
            .arg(processId);
    auto file = std::make_unique<QFile>(QDir(directory).filePath(fileName));

    // NewOnly creates the file exclusively, so an existing file or symlink is
    // never followed or truncated in a shared log directory.
    const QIODevice::OpenMode mode = QIODevice::WriteOnly |
            QIODevice::Text | QIODevice::NewOnly;
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    if (!file->open(mode, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
#else
    if (!file->open(mode)) {
#endif
        return {};
    }

    return file;
}

}
