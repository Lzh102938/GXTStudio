#include "GXTStudio.h"
#include <QtWidgets/QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QFont>
#include <QFontDatabase>
#include <QSurfaceFormat>
#include <QTimer>
#include <QDir>
#include <QDebug>

// 全局变量保存字体ID - 供其他模块使用
int g_fontAwesomeSolidId = -1;

// 加载 Font Awesome 图标字体
static bool loadFontAwesome()
{
    // 使用可执行文件所在目录，而不是当前工作目录
    QString fontPath = QCoreApplication::applicationDirPath() + "/font/";
    qDebug() << "Font Awesome path:" << fontPath;
    qDebug() << "Application directory:" << QCoreApplication::applicationDirPath();

    QFontDatabase db;
    int loadedCount = 0;

    // 加载 Solid 字体
    QString solidFont = fontPath + "Font Awesome 7 Free-Solid-900.otf";
    qDebug() << "Loading Solid font from:" << solidFont;
    g_fontAwesomeSolidId = db.addApplicationFont(solidFont);
    if (g_fontAwesomeSolidId != -1) {
        loadedCount++;
        QString familyName = db.applicationFontFamilies(g_fontAwesomeSolidId).join(", ");
        qDebug() << "Font Awesome 7 Free-Solid loaded successfully.";
        qDebug() << "  Family:" << familyName << "ID:" << g_fontAwesomeSolidId;
        qDebug() << "  Test char f031:" << QChar(0xf031);
    } else {
        qDebug() << "Failed to load Font Awesome 7 Free-Solid from:" << solidFont;
    }

    qDebug() << "Loaded" << loadedCount << "Font Awesome font(s)";

    // 验证字体是否真正可用 - 直接从加载的字体ID获取字体
    qDebug() << "=== Font Verification ===";
    if (g_fontAwesomeSolidId != -1) {
        QStringList solidFamilies = db.applicationFontFamilies(g_fontAwesomeSolidId);
        qDebug() << "Solid font families:" << solidFamilies;
    }

    return loadedCount > 0;
}

int main(int argc, char *argv[])
{
    // 强制使用 FreeType 字体引擎
    qputenv("QT_QPA_PLATFORM", QByteArray("windows:fontengine=freetype"));
    
    // FreeType 字体渲染优化参数
    qputenv("QT_FONT_DPI", QByteArray("96"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));
    
    // 极致性能优化设置
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_NativeWindows);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    
    QApplication app(argc, argv);
    
    // 加载 Font Awesome 图标字体
    loadFontAwesome();
    
    // FreeType 字体渲染优化配置
    QFont appFont = app.font();
    appFont.setFamily("Microsoft YaHei");
    appFont.setPointSize(9);
    // FreeType 最佳渲染设置
    appFont.setHintingPreference(QFont::PreferFullHinting);  // 完整微调，FreeType最佳效果
    appFont.setStyleStrategy(QFont::PreferAntialias);        // 优先抗锯齿
    appFont.setStyleStrategy(QFont::PreferMatch);            // 精确匹配
    appFont.setKerning(true);  // 启用字距调整，FreeType支持良好
    appFont.setFixedPitch(false);
    app.setFont(appFont);
    
    // 极致图形渲染优化 - 修复黑色块闪烁
    QSurfaceFormat format;
    format.setDepthBufferSize(24); // 恢复深度缓冲防止闪烁
    format.setStencilBufferSize(8);  // 恢复模板缓冲
    format.setSamples(4); // 启用抗锯齿减少闪烁
    format.setSwapInterval(1); // 启用垂直同步防止撕裂
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(8); // 启用Alpha通道
    format.setProfile(QSurfaceFormat::OpenGLContextProfile::CompatibilityProfile); // 兼容性配置
    QSurfaceFormat::setDefaultFormat(format);
    
    // 优化Qt引擎设置 - 修复黑色块闪烁
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", QByteArray("1"));
    qputenv("QT_USE_NATIVE_WINDOWS", QByteArray("1"));
    qputenv("QT_DEVICE_PIXEL_RATIO", QByteArray("auto"));
    qputenv("QT_HIGHDPI_SCALE_FACTOR_ROUNDING_POLICY", QByteArray("PassThrough"));
    qputenv("QT_SCALE_FACTOR", QByteArray("1")); // 固定缩放因子
    qputenv("QT_OPENGL", QByteArray("desktop")); // 强制桌面OpenGL
    qputenv("QT_LOGGING_RULES", QByteArray("qt.qpa.*=false")); // 禁用日志
    qputenv("QT_DEBUG_PLUGINS", QByteArray("0")); // 禁用插件调试
    // 移除可能导致闪烁的设置
    // qputenv("QT_QUICK_BACKEND", QByteArray("software"));
    // qputenv("QT_WIDGETS_RHI", QByteArray("1"));
    // qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));
    
    // 选择性禁用UI动画 - 保留必要的动画减少闪烁
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
    // 保留渐变动画防止闪烁
    // QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    // QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    // QApplication::setEffectEnabled(Qt::UI_AnimateToolBox, false);
    
    // 极致性能设置
    QApplication::setDesktopSettingsAware(false); // 忽略桌面设置提升一致性
    QApplication::setWheelScrollLines(3); // 减少滚轮滚动行数
    
    // 极致内存优化
    QCoreApplication::processEvents(); // 预处理事件
    app.setQuitOnLastWindowClosed(true); // 最后一窗口关闭时退出
    
    GXTStudio window;
    window.show();
    
    return app.exec();
}