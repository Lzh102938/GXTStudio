#include "GXTStyleManager.h"
#include "FontAwesome.h"
#include <QColor>

GXTStyleManager& GXTStyleManager::instance()
{
    static GXTStyleManager inst;
    return inst;
}

void GXTStyleManager::initialize()
{
    if (m_cachedStyles.initialized) return;

    QString fontFamily = FA::solidFontFamily();

    m_cachedStyles.tabWidgetStyle = R"(
        QTabWidget::pane {
            border: none;
            background: transparent;
            top: 0px;
        }
        QTabBar {
            background: transparent;
            qproperty-drawBase: false;
        }
        QTabBar::tab {
            background: rgba(255, 255, 255, 0.65);
            border: none;
            border-radius: 16px;
            padding: 8px 16px;
            margin-right: 4px;
            margin-left: 2px;
            color: rgba(60, 70, 85, 0.75);
            font-weight: 500;
            min-width: 80px;
        }
        QTabBar::tab:selected {
            background: rgba(255, 255, 255, 0.92);
            color: #1976d2;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            background: rgba(255, 255, 255, 0.82);
            color: rgba(33, 150, 243, 0.9);
        }
        QTabBar::tab:!selected {
            margin-top: 3px;
        }
        QTabBar::close-button {
            image: none;
            subcontrol-position: right;
            margin-right: 2px;
            margin-left: 4px;
        }
    )";

    m_cachedStyles.toolBarStyle = R"(
        QToolBar {
            background: #f8f9fa;
            border: none;
            border-bottom: 1px solid #e0e0e0;
            padding: 4px 8px;
            spacing: 4px;
        }
    )";

    m_cachedStyles.toolButtonStyle = R"(
        QToolButton {
            background: transparent;
            border: none;
            border-radius: 4px;
            padding: 6px 10px;
            color: #495057;
        }
        QToolButton:hover {
            background: rgba(91, 159, 214, 0.15);
            color: #5b9fd6;
        }
        QToolButton:pressed {
            background: rgba(91, 159, 214, 0.25);
        }
        QToolButton::menu-indicator {
            image: none;
            subcontrol-position: right center;
            subcontrol-origin: padding;
            left: -4px;
            width: 12px;
        }
    )";

    m_cachedStyles.splitterStyle = R"(
        QSplitter::handle {
            background: transparent;
            border: none;
            width: 6px;
        }
        QSplitter::handle:hover {
            background: rgba(91, 159, 214, 0.3);
        }
        QSplitter::handle:pressed {
            background: rgba(91, 159, 214, 0.5);
        }
    )";

    m_cachedStyles.tableListStyle = R"(
        QListWidget{border:1px solid #dee2e6;border-radius:8px;background-color:rgba(255,255,255,0.5);padding:6px;outline:none;}
        QListWidget::item{border:1px solid #e9ecef;border-radius:6px;background-color:transparent;padding:10px 12px;margin:2px 1px;min-height:18px;}
        QListWidget::item:selected{background-color:rgba(187,222,251,0.9);border-color:#1976d2;color:#0d47a1;font-weight:500;}
        QListWidget::item:hover{background-color:rgba(227,242,253,0.6);border-color:#90caf9;}
        QListWidget::item:selected:hover{background-color:rgba(144,202,249,0.95);}
        QScrollBar:vertical{width:12px;background:transparent;margin:0px;}
        QScrollBar::handle:vertical{background:rgba(150,150,150,0.6);min-height:30px;border-radius:6px;margin:2px;}
        QScrollBar::handle:vertical:hover{background:rgba(120,120,120,0.8);}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0px;background:none;}
        QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:none;}
    )";

    m_cachedStyles.tableViewStyle = R"(
        QTableView{border:none;background-color:rgba(255,255,255,0.5);gridline-color:#f0f0f0;outline:none;selection-background-color:#bbdefb;selection-color:#0d47a1;}
        QTableView::item{padding:6px 8px;border-bottom:1px solid #f0f0f0;border-right:1px solid #f0f0f0;outline:none;}
        QTableView::item:selected{background-color:#bbdefb;color:#0d47a1;}
        QTableView::item:hover{background-color:#e3f2fd;}
        QHeaderView::section{background-color:rgba(245,245,245,0.5);border:1px solid #ddd;border-bottom:2px solid #ccc;padding:4px 8px;font-weight:bold;font-size:13px;color:#333;text-align:center;}
        QHeaderView::section:first{min-width:120px;}
        QHeaderView::section:hover{background-color:rgba(232,232,232,0.5);}
        QHeaderView{background-color:transparent;}
    )";

    m_cachedStyles.searchEditStyleTemplate =
        "QLineEdit{border:2px solid #e1e5e9;border-radius:6px;padding:6px 12px;padding-left:14px;background-color:rgba(255,255,255,0.5);font-size:12px;color:%1;}"
        "QLineEdit:focus{border-color:#2196f3;outline:none;}";

    m_cachedStyles.navButtonStyleTemplate =
        "QPushButton{background-color:rgba(255,255,255,0.5);border:1px solid #dee2e6;border-radius:6px;color:%1;font-weight:bold;}"
        "QPushButton:hover{background-color:rgba(255,255,255,0.7);border-color:#adb5bd;}"
        "QPushButton:pressed{background-color:rgba(255,255,255,0.9);}";

    m_cachedStyles.labelStyleTemplate =
        "QLabel{font-weight:bold;font-size:14px;color:%1;padding:6px 8px;background-color:transparent;border-radius:6px;border:none;}";

    m_cachedStyles.iconButtonStyleTemplate =
        "QToolButton{border:none;border-radius:4px;background-color:transparent;color:%1;font-family:'" + fontFamily + "';font-size:13px;}"
        "QToolButton:hover{background-color:rgba(255,255,255,0.5);color:%1;}"
        "QToolButton:pressed{background-color:#e9ecef;}"
        "QToolButton:checked{color:#2196f3;background-color:#e3f2fd;}"
        "QToolButton:checked:hover{background-color:#bbdefb;}";

    QString buttonColor = "#3498db";
    QString hoverColor = QColor(buttonColor).darker(120).name();
    QString pressedColor = QColor(buttonColor).darker(150).name();
    m_cachedStyles.addEntryButtonStyle = QString(
        "QPushButton{background-color:%1;color:white;border:none;border-radius:6px;padding:0px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background-color:%2;}"
        "QPushButton:pressed{background-color:%3;}"
        "QPushButton:disabled{background-color:#e0e0e0;color:#a0a0a0;}"
    ).arg(buttonColor).arg(hoverColor).arg(pressedColor);

    m_cachedStyles.initialized = true;
}

QString GXTStyleManager::getTabWidgetStyle() const
{
    return m_cachedStyles.tabWidgetStyle;
}

QString GXTStyleManager::getToolBarStyle() const
{
    return m_cachedStyles.toolBarStyle;
}

QString GXTStyleManager::getToolButtonStyle() const
{
    return m_cachedStyles.toolButtonStyle;
}

QString GXTStyleManager::getSplitterStyle() const
{
    return m_cachedStyles.splitterStyle;
}

QString GXTStyleManager::getTableListStyle() const
{
    return m_cachedStyles.tableListStyle;
}

QString GXTStyleManager::getTableViewStyle() const
{
    return m_cachedStyles.tableViewStyle;
}

QString GXTStyleManager::getAddEntryButtonStyle() const
{
    return m_cachedStyles.addEntryButtonStyle;
}

QString GXTStyleManager::getSearchEditStyle(const QColor& textColor)
{
    return m_cachedStyles.searchEditStyleTemplate.arg(textColor.name());
}

QString GXTStyleManager::getNavButtonStyle(const QColor& textColor)
{
    return m_cachedStyles.navButtonStyleTemplate.arg(textColor.name());
}

QString GXTStyleManager::getLabelStyle(const QColor& textColor)
{
    return m_cachedStyles.labelStyleTemplate.arg(textColor.name());
}

QString GXTStyleManager::getIconButtonStyle(const QColor& textColor)
{
    return m_cachedStyles.iconButtonStyleTemplate.arg(textColor.name());
}

QString GXTStyleManager::getCardStyle(const QString& color) const
{
    return QString(R"(
        QWidget {
            background-color: white;
            border-radius: 10px;
            border: 1px solid #e0e0e0;
            padding: 15px;
        }
        QWidget:hover {
            border: 2px solid %1;
            padding: 14px;
        }
    )").arg(color);
}

QString GXTStyleManager::getButtonStyle(const QString& color, bool darker) const
{
    QString bgColor = darker ? QColor(color).darker(120).name() : color;
    QString pressedColor = darker ? QColor(color).darker(150).name() : bgColor;
    return QString(R"(
        QToolButton {
            background-color: %1;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 12px 16px;
            font-size: 14px;
            font-weight: bold;
            text-align: left;
        }
        QToolButton:hover {
            background-color: %2;
        }
        QToolButton:pressed {
            background-color: %3;
        }
    )").arg(color).arg(bgColor).arg(pressedColor);
}

QString GXTStyleManager::formatKeyForDisplay(const QString& key)
{
    if (key.length() == 8) {
        bool allHex = true;
        for (const QChar& c : key) {
            if (!c.isDigit() && (c.toLower() < 'a' || c.toLower() > 'f')) {
                allHex = false;
                break;
            }
        }
        if (allHex) {
            return "0x" + key;
        }
    }
    return key;
}
