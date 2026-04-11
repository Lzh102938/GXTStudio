#pragma once

#include <QString>
#include <QColor>

class GXTStyleManager
{
public:
    static GXTStyleManager& instance();

    void initialize();

    QString getTabWidgetStyle() const;
    QString getToolBarStyle() const;
    QString getToolButtonStyle() const;
    QString getSplitterStyle() const;
    QString getTableListStyle() const;
    QString getTableViewStyle() const;
    QString getAddEntryButtonStyle() const;

    QString getSearchEditStyle(const QColor& textColor);
    QString getNavButtonStyle(const QColor& textColor);
    QString getLabelStyle(const QColor& textColor);
    QString getIconButtonStyle(const QColor& textColor);

    QString getCardStyle(const QString& color) const;
    QString getButtonStyle(const QString& color, bool darker = true) const;

    static QString formatKeyForDisplay(const QString& key);

private:
    GXTStyleManager() = default;
    GXTStyleManager(const GXTStyleManager&) = delete;
    GXTStyleManager& operator=(const GXTStyleManager&) = delete;

    struct CachedStyles {
        QString tabWidgetStyle;
        QString toolBarStyle;
        QString toolButtonStyle;
        QString splitterStyle;
        QString tableListStyle;
        QString tableViewStyle;
        QString searchEditStyleTemplate;
        QString navButtonStyleTemplate;
        QString labelStyleTemplate;
        QString addEntryButtonStyle;
        QString iconButtonStyleTemplate;
        bool initialized = false;
    } m_cachedStyles;
};
