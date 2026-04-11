#include "GXTVersionUtils.h"
#include <QCoreApplication>
#include <QFile>

QIcon GXTVersionUtils::getVersionIcon(GXTVersion version)
{
    QString iconPath;
    switch (version) {
        case GXTVersion::GTA_III:
            iconPath = "icon/III.ico";
            break;
        case GXTVersion::GTA_VC:
            iconPath = "icon/VC.ico";
            break;
        case GXTVersion::GTA_SA:
            iconPath = "icon/SA.ico";
            break;
        case GXTVersion::GTA_IV:
            iconPath = "icon/IV.ico";
            break;
        case GXTVersion::GXT2:
            iconPath = "icon/V.ico";
            break;
        default:
            return QIcon();
    }
    
    QString fullPath = QCoreApplication::applicationDirPath() + "/" + iconPath;
    if (QFile::exists(fullPath)) {
        return QIcon(fullPath);
    }
    return QIcon();
}

std::string GXTVersionUtils::getVersionName(GXTVersion version)
{
    switch (version) {
        case GXTVersion::GTA_III: return "GTA III";
        case GXTVersion::GTA_VC: return "GTA Vice City";
        case GXTVersion::GTA_SA: return "GTA San Andreas";
        case GXTVersion::GTA_IV: return "GTA IV";
        case GXTVersion::GXT2: return "GXT2";
        default: return "Unknown";
    }
}

QString GXTVersionUtils::getVersionString(GXTVersion version)
{
    switch (version) {
        case GXTVersion::GTA_III: return "GTA III";
        case GXTVersion::GTA_VC: return "GTA Vice City";
        case GXTVersion::GTA_SA: return "GTA San Andreas";
        case GXTVersion::GTA_IV: return "GTA IV";
        case GXTVersion::GXT2: return "GXT2";
        default: return "Unknown";
    }
}

std::string GXTVersionUtils::getVersionCompatibilityInfo(GXTVersion version)
{
    switch (version) {
        case GXTVersion::GTA_III:
            return "GTA III格式：可转换为GTA VC格式";
        case GXTVersion::GTA_VC:
            return "GTA VC格式：可转换为所有GTA III/SA/IV格式";
        case GXTVersion::GTA_SA:
            return "GTA SA格式：8位字符，可转换为VC/IV格式";
        case GXTVersion::GTA_IV:
            return "GTA IV格式：16位字符，可转换为VC/SA格式";
        case GXTVersion::GXT2:
            return "GXT2格式：GTA V专用，不与其他格式兼容";
        default:
            return "未知格式";
    }
}
