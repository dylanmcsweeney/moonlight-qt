#pragma once

#include <QString>

class BuildIdentity
{
public:
    static QString buildLabel();
    static QString displayVersion(const QString& numericVersion,
                                  const QString& label);
    static QString displayVersion();
    static bool isCustomBuild();
};
