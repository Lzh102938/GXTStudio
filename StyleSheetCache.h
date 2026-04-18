#pragma once

#include <QString>
#include <QHash>
#include <QStringBuilder>

class StyleSheetCache {
private:
    static QHash<QString, QString> s_styleTemplateCache;
    static QHash<QString, QString> s_styleCache;
    static bool s_initialized;
    
public:
    static void initialize() {
        if (s_initialized) return;
        
        s_styleTemplateCache["searchEdit"] = QString(
            "QLineEdit {"
            "  border: 2px solid rgba(200, 200, 200, 0.5);"
            "  border-radius: 6px;"
            "  padding: 6px 12px;"
            "  padding-left: 14px;"
            "  background-color: rgba(255, 255, 255, 0.5);"
            "  font-size: 12px;"
            "  color: %1;"
            "}"
            "QLineEdit:focus {"
            "  border-color: rgba(33, 150, 243, 0.6);"
            "  outline: none;"
            "}"
        );
        
        s_styleTemplateCache["label"] = QString(
            "QLabel{font-weight:bold;font-size:14px;color:%1;padding:6px 8px;background-color:transparent;border-radius:6px;border:none;}"
        );
        
        s_styleTemplateCache["labelWithMargin"] = QString(
            "margin-left:6px; color: %1;"
        );
        
        s_styleTemplateCache["labelSmall"] = QString(
            "color: %1; font-size: 12px;"
        );
        
        s_styleTemplateCache["toolButton"] = QString(
            "QToolButton {"
            "  background: transparent;"
            "  border: none;"
            "  border-radius: 4px;"
            "  color: %1;"
            "  padding: 0px;"
            "  margin: 0px;"
            "}"
            "QToolButton:hover {"
            "  background: rgba(0, 0, 0, 0.08);"
            "}"
            "QToolButton:checked {"
            "  background: rgba(33, 150, 243, 0.15);"
            "  color: #2196f3;"
            "}"
        );
        
        s_styleTemplateCache["iconLabel"] = QString(
            "font-family:'%1'; font-size: 32px; color: %2;"
        );
        
        s_styleTemplateCache["iconLabelSimple"] = QString(
            "font-size: 32px; color: %1;"
        );
        
        s_styleTemplateCache["valueLabel"] = QString(
            "font-size: 22px; font-weight: 800; color: %1;"
        );
        
        s_styleTemplateCache["iconLabelBlue"] = QString(
            "font-family: '%1'; font-size: 32px; color: #17a2b8;"
        );
        
        s_initialized = true;
    }
    
    static QString getStyle(const QString& templateName, const QString& param1 = QString(), const QString& param2 = QString()) {
        initialize();
        
        QString cacheKey = templateName;
        if (!param1.isEmpty()) {
            cacheKey += "_" + param1;
        }
        if (!param2.isEmpty()) {
            cacheKey += "_" + param2;
        }
        
        auto it = s_styleCache.find(cacheKey);
        if (it != s_styleCache.end()) {
            return it.value();
        }
        
        QString templateStr = s_styleTemplateCache.value(templateName);
        if (templateStr.isEmpty()) {
            return QString();
        }
        
        QString style;
        if (!param2.isEmpty()) {
            style = templateStr.arg(param1, param2);
        } else if (!param1.isEmpty()) {
            style = templateStr.arg(param1);
        } else {
            style = templateStr;
        }
        
        s_styleCache[cacheKey] = style;
        return style;
    }
    
    static void clearCache() {
        s_styleCache.clear();
    }
    
    static int getCacheSize() {
        return s_styleCache.size();
    }
    
    static int getTemplateCount() {
        return s_styleTemplateCache.size();
    }
};
