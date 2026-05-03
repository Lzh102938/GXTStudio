#include "GXTStudio.h"
#include "UndoSystem.h"
#include <QtWidgets/QApplication>
#include <QByteArray>
#include <QGuiApplication>
#include <QFont>
#include <QFontDatabase>
#include <QSurfaceFormat>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QObject>

// 全局变量保存字体ID - 供其他模块使用
int g_fontAwesomeSolidId = -1;
int g_fontAwesomeBrandsId = -1;

// 后台线程加载字体
class FontLoader : public QObject
{
    Q_OBJECT
public:
    explicit FontLoader(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void loadFonts()
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
        } else {
            qDebug() << "Failed to load Font Awesome 7 Free-Solid from:" << solidFont;
        }

        // 加载 Brands 字体
        QString brandsFont = fontPath + "Font Awesome 7 Brands-Regular-400.otf";
        qDebug() << "Loading Brands font from:" << brandsFont;
        g_fontAwesomeBrandsId = db.addApplicationFont(brandsFont);
        if (g_fontAwesomeBrandsId != -1) {
            loadedCount++;
            QString familyName = db.applicationFontFamilies(g_fontAwesomeBrandsId).join(", ");
            qDebug() << "Font Awesome 7 Brands loaded successfully.";
            qDebug() << "  Family:" << familyName << "ID:" << g_fontAwesomeBrandsId;
        } else {
            qDebug() << "Failed to load Font Awesome 7 Brands from:" << brandsFont;
        }

        qDebug() << "Loaded" << loadedCount << "Font Awesome font(s)";

        // 验证字体是否真正可用 - 直接从加载的字体ID获取字体
        qDebug() << "=== Font Verification ===";
        if (g_fontAwesomeSolidId != -1) {
            QStringList solidFamilies = db.applicationFontFamilies(g_fontAwesomeSolidId);
            qDebug() << "Solid font families:" << solidFamilies;
        }

        emit fontsLoaded(loadedCount > 0);
    }

signals:
    void fontsLoaded(bool success);
};

// 启动字体加载线程
void startFontLoading()
{
    QThread *fontThread = new QThread();
    FontLoader *fontLoader = new FontLoader();
    fontLoader->moveToThread(fontThread);

    QObject::connect(fontThread, &QThread::started, fontLoader, &FontLoader::loadFonts);
    QObject::connect(fontLoader, &FontLoader::fontsLoaded, [fontThread, fontLoader](bool success) {
        qDebug() << "Font loading completed, success:" << success;
        fontLoader->deleteLater();
        fontThread->quit();
        fontThread->wait(1000);
        fontThread->deleteLater();
    });

    fontThread->start();
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArray("windows:fontengine=freetype"));
    
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, false);
    
    QApplication app(argc, argv);
    
    // 注册自定义类型用于撤销系统
    qRegisterMetaType<CellEditData>("CellEditData");
    qRegisterMetaType<EntryData>("EntryData");
    qRegisterMetaType<MultipleEntriesData>("MultipleEntriesData");
    qRegisterMetaType<TableData>("TableData");
    
    // 后台加载 Font Awesome 图标字体
    startFontLoading();
    
    // 设置应用程序图标
    QString iconPath = QCoreApplication::applicationDirPath() + "/icon/favicon.ico";
    QIcon appIcon(iconPath);
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
        qDebug() << "Application icon loaded successfully:" << iconPath;
    } else {
        qDebug() << "Failed to load application icon:" << iconPath;
    }
    
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
    
    QSurfaceFormat format;
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(0);
    format.setSamples(0);
    format.setSwapInterval(1);
    format.setRedBufferSize(8);
    format.setGreenBufferSize(8);
    format.setBlueBufferSize(8);
    format.setAlphaBufferSize(0);
    format.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", QByteArray("1"));
    qputenv("QT_OPENGL", QByteArray("desktop"));
    qputenv("QT_LOGGING_RULES", QByteArray("qt.qpa.*=false"));
    
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
    
    QApplication::setDesktopSettingsAware(false);
    QApplication::setWheelScrollLines(3);
    
    app.setQuitOnLastWindowClosed(true);
    
    GXTStudio window;
    window.show();
    
    for (int i = 1; i < argc; ++i) {
        QString filePath = QString::fromLocal8Bit(argv[i]);
        QFileInfo fi(filePath);
        if (fi.exists() && fi.isFile()) {
            QTimer::singleShot(100, &window, [&window, filePath]() {
                window.openFileFromPath(filePath);
            });
        }
    }
    
    return app.exec();
}

#include "main.moc"