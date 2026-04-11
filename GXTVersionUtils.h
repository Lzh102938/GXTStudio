#pragma once

#include <QString>
#include <QIcon>
#include <string>
#include "GXTParser.h"

class GXTVersionUtils
{
public:
    static std::string getVersionName(GXTVersion version);
    static QString getVersionString(GXTVersion version);
    static QIcon getVersionIcon(GXTVersion version);
    static std::string getVersionCompatibilityInfo(GXTVersion version);
};
