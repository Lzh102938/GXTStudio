#ifndef VERSIONINFO_H
#define VERSIONINFO_H

#include <QString>
#include <QVector>

// 版本信息类
class VersionInfo {
private:
    VersionInfo() {} // 私有构造函数，防止实例化

public:
    // 当前版本号
    static const QString currentVersion;
    
    // 版本信息结构体
    struct VersionEntry {
        QString version;
        QString date;
        QString changes;
    };
    
    // 获取版本历史
    static QVector<VersionEntry> getVersionHistory();
};

#endif // VERSIONINFO_H
