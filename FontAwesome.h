#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QChar>
#include <QStringList>
#include <QString>

// 外部声明 Font Awesome 字体 ID（从 main.cpp 导出）
extern int g_fontAwesomeSolidId;
extern int g_fontAwesomeBrandsId;

/**
 * @brief Font Awesome 图标工具类
 * 
 * 提供 Font Awesome 7 Free Solid 图标的 Unicode 定义和字体获取功能
 * 使用方式：
 *   - 直接访问图标: FA::Search, FA::QSave 等
 *   - 获取字体: FA::solidFont(14)  // 14是字体大小
 *   - 获取字体族名: FA::solidFontFamily()
 */
namespace FA {
    // Font Awesome 7 图标 Unicode
    constexpr QChar Search = QChar(0xf002);                    // 搜索图标 (Solid)
    constexpr QChar CaseSensitive = QChar(0x0041);            // 大写字母 A (Solid)
    constexpr QChar Regex = QChar(0xf069);                    // asterisk 图标 (Solid)
    constexpr QChar QChevronLeft = QChar(0xf053);          // 左箭头 (Solid) - 上一个
    constexpr QChar QChevronRight = QChar(0xf054);           // 右箭头 (Solid) - 下一个
    constexpr QChar QChevronDown = QChar(0xf078);        // 下箭头 (Solid) - 展开
    constexpr QChar QChevronUp = QChar(0xf077);          // 上箭头 (Solid) - 收起
    constexpr QChar QCheck = QChar(0xf00c);                   // 勾选图标 (Solid) - 成功
    constexpr QChar QTimes = QChar(0xf00d);                   // 叉号图标 (Solid) - 失败
    constexpr QChar QTrash = QChar(0xf1f8);                   // 垃圾桶图标 (Solid) - 删除
    constexpr QChar QExclamationTriangle = QChar(0xf071);    // 三角警告图标 (Solid) - 警告
    constexpr QChar QChartBar = QChar(0xf080);                // 条形图 (Solid) - 统计
    constexpr QChar QLineChart = QChar(0xf201);                // 折线图 (Solid) - 趋势
    constexpr QChar QEdit = QChar(0xf15b);                     // 编辑图标 (Solid) - 编辑
    constexpr QChar QGift = QChar(0xf02d);                      // 礼物图标 (Solid) - 庆祝
    constexpr QChar QCog = QChar(0xf013);                         // 齿轮图标 (Solid) - 设置/配置
    constexpr QChar QKey = QChar(0xf084);                          // 钥匙图标 (Solid) - API 密钥
    constexpr QChar QPen = QChar(0xf304);                           // 笔图标 (Solid) - 提示词
    constexpr QChar QSync = QChar(0xf021);                          // 同步/加载图标 (Solid)
    constexpr QChar QHourglassHalf = QChar(0xf252);              // 沙漏图标 (Solid) - 等待/测试中
    constexpr QChar QLightbulb = QChar(0xf0eb);                     // 灯泡图标 (Solid) - 提示/帮助
    constexpr QChar QFolder = QChar(0xf07b);                       // 文件夹图标 (Solid)
    constexpr QChar QWrench = QChar(0xf0ad);                      // 扳手图标 (Solid) - 工具/编辑
    constexpr QChar QClipboardList = QChar(0xf46d);            // 列表图标 (Solid) - 管理
    constexpr QChar QSearchMagnifyingGlass = QChar(0xf002);   // 放大镜图标 (Solid) - 搜索
    constexpr QChar QSave = QChar(0xf0c7);                         // 保存图标 (Solid)
    constexpr QChar QBullseye = QChar(0xf140);                     // 靶心图标 (Solid) - 性能/目标
    constexpr QChar QPalette = QChar(0xf53f);                     // 调色板图标 (Solid) - 界面/设计
    constexpr QChar QGamepadModern = QChar(0xf5cc);          // 现代游戏手柄图标 (Solid) - 主标题
    constexpr QChar QBoltLightning = QChar(0xf0e7);        // 闪电图标 (Solid) - 快速操作
    constexpr QChar QFileAlt = QChar(0xf15c);                    // 文件图标 (Solid) - 文件格式
    constexpr QChar QKeyboard = QChar(0xf11c);                  // 键盘图标 (Solid) - 快捷键
    constexpr QChar QGlobe = QChar(0xf0ac);                      // 地球图标 (Solid) - 游戏平台
    constexpr QChar QTable = QChar(0xf0ce);                      // 表格图标 (Solid) - 表格
    constexpr QChar QDatabase = QChar(0xf1c0);                  // 数据库图标 (Solid) - 数据
    constexpr QChar QLanguage = QChar(0xf1ab);                  // 语言图标 (Solid) - 语言
    constexpr QChar QBuilding = QChar(0xf1ad);                  // 建筑图标 (Solid) - 团队
    constexpr QChar QCodeBranch = QChar(0xf126);              // 代码分支图标 (Solid) - 技术
    constexpr QChar QTag = QChar(0xf02b);                        // 标签图标 (Solid) - 版本
    constexpr QChar QPlus = QChar(0xf102);                       // 加号图标 (Solid) - 添加
    constexpr QChar QFolderOpen = QChar(0xf07c);              // 打开的文件夹 (Solid) - 打开文件
    constexpr QChar QUpload = QChar(0xf093);                    // 上传图标 (Solid) - 拖拽上传
    constexpr QChar QInfoCircle = QChar(0xf05a);              // 信息圆圈 (Solid) - 关于信息
    constexpr QChar QFileCode = QChar(0xf1c9);                  // 代码文件图标 (Solid) - 格式
    constexpr QChar QCalendar = QChar(0xf133);               // 日历图标 (Solid) - 日期
    constexpr QChar QUser = QChar(0xf007);                     // 用户图标 (Solid) - 作者/开发者
    constexpr QChar QLink = QChar(0xf0c1);                     // 链接图标 (Solid) - 外部链接
    constexpr QChar QUserGroup = QChar(0xf0c0);                // 用户组图标 (Solid) - 团队组织
    constexpr QChar QHeart = QChar(0xf004);                    // 心形图标 (Solid) - 特别鸣谢
    
    // Font Awesome 7 Brands 图标 Unicode
    constexpr QChar QWeixin = QChar(0xf1d7);                   // 微信图标 (Brands) - 捐赠
    constexpr QChar QAlipay = QChar(0xf642);                   // 支付宝图标 (Brands) - 捐赠

    /**
     * @brief 获取 Font Awesome 7 Free Solid 字体族名称
     * @return 字体族名称，如果字体未加载则返回空字符串
     */
    inline QString solidFontFamily() {
        static QString cachedFamily;
        if (!cachedFamily.isEmpty()) {
            return cachedFamily;
        }
        
        QFontDatabase db;
        if (g_fontAwesomeSolidId != -1) {
            QStringList families = db.applicationFontFamilies(g_fontAwesomeSolidId);
            if (!families.isEmpty()) {
                cachedFamily = families.first();
                return cachedFamily;
            }
        }
        
        cachedFamily = QStringLiteral("Font Awesome 7 Free");
        return cachedFamily;
    }

    /**
     * @brief 获取 Font Awesome 7 Brands 字体族名称
     * @return 字体族名称，如果字体未加载则返回空字符串
     */
    inline QString brandsFontFamily() {
        static QString cachedFamily;
        if (!cachedFamily.isEmpty()) {
            return cachedFamily;
        }
        
        QFontDatabase db;
        if (g_fontAwesomeBrandsId != -1) {
            QStringList families = db.applicationFontFamilies(g_fontAwesomeBrandsId);
            if (!families.isEmpty()) {
                cachedFamily = families.first();
                return cachedFamily;
            }
        }
        
        cachedFamily = QStringLiteral("Font Awesome 7 Brands");
        return cachedFamily;
    }

    /**
     * @brief 获取 Font Awesome 7 Free Solid 字体
     * @param size 字体大小（点数），默认14
     * @return QFont 对象，如果 Font Awesome 未加载则返回默认字体
     */
    inline QFont solidFont(int size = 14) {
        QString family = solidFontFamily();
        QFont font(family);
        font.setPointSize(size);
        font.setBold(true);
        font.setStyleStrategy(QFont::PreferAntialias);
        return font;
    }

    /**
     * @brief 获取 Font Awesome 7 Brands 字体
     * @param size 字体大小（点数），默认14
     * @return QFont 对象，如果 Font Awesome 未加载则返回默认字体
     */
    inline QFont brandsFont(int size = 14) {
        QString family = brandsFontFamily();
        QFont font(family);
        font.setPointSize(size);
        font.setStyleStrategy(QFont::PreferAntialias);
        return font;
    }
}
