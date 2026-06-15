#include "buildidentity.h"

#include <QFile>

QString BuildIdentity::buildLabel()
{
    QFile file(QStringLiteral(":/buildlabel.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString BuildIdentity::displayVersion(const QString& numericVersion,
                                      const QString& label)
{
    const QString trimmedLabel = label.trimmed();
    if (trimmedLabel.isEmpty()) {
        return numericVersion;
    }
    if (trimmedLabel.startsWith(QLatin1Char('('))) {
        return numericVersion + QLatin1Char(' ') + trimmedLabel;
    }
    return numericVersion + QLatin1Char('-') + trimmedLabel;
}

QString BuildIdentity::displayVersion()
{
    return displayVersion(QStringLiteral(VERSION_STR), buildLabel());
}

bool BuildIdentity::isCustomBuild()
{
    return !buildLabel().isEmpty();
}
