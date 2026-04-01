#pragma once

#include <QString>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>
#include <QDataStream>
#include <QDebug>
#include <QVariantMap>

class AppConfig
{
public:
    static AppConfig& instance()
    {
        static AppConfig instance;
        return instance;
    }

    void load()
    {
        QString configPath = getConfigFilePath();
        
        QFile file(configPath);
        if (!file.exists()) {
            migrateFromQSettings();
            return;
        }
        
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open config file:" << configPath;
            return;
        }
        
        QByteArray encryptedData = file.readAll();
        file.close();
        
        QByteArray decryptedData = decryptData(encryptedData);
        
        QDataStream in(&decryptedData, QIODevice::ReadOnly);
        in.setVersion(QDataStream::Qt_6_0);
        
        quint32 magic;
        in >> magic;
        
        if (magic != CONFIG_MAGIC) {
            qWarning() << "Invalid config file format";
            return;
        }
        
        quint32 version;
        in >> version;
        
        if (version > CONFIG_VERSION) {
            qWarning() << "Config file version too new:" << version;
            return;
        }
        
        in >> m_config;
        
        qDebug() << "Config loaded from:" << configPath;
    }

    void save()
    {
        QString configPath = getConfigFilePath();
        
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        
        out << CONFIG_MAGIC;
        out << CONFIG_VERSION;
        out << m_config;
        
        QByteArray encryptedData = encryptData(data);
        
        QFile file(configPath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create config file:" << configPath;
            return;
        }
        
        file.write(encryptedData);
        file.close();
        
        qDebug() << "Config saved to:" << configPath;
    }

    QString getConfigFilePath() const
    {
        QString appDir = QCoreApplication::applicationDirPath();
        return appDir + "/config.dat";
    }

    QString getBackgroundImagePath() const
    {
        return m_config.value("background/imagePath").toString();
    }

    void setBackgroundImagePath(const QString& path)
    {
        m_config["background/imagePath"] = path;
    }

    int getBackgroundBlurRadius() const
    {
        return m_config.value("background/blurRadius", 0).toInt();
    }

    void setBackgroundBlurRadius(int radius)
    {
        m_config["background/blurRadius"] = radius;
    }

    int getBackgroundBrightness() const
    {
        return m_config.value("background/brightness", 0).toInt();
    }

    void setBackgroundBrightness(int brightness)
    {
        m_config["background/brightness"] = brightness;
    }

    double getBackgroundOpacity() const
    {
        return m_config.value("background/opacity", 1.0).toDouble();
    }

    void setBackgroundOpacity(double opacity)
    {
        m_config["background/opacity"] = opacity;
    }

    bool isBackgroundEnabled() const
    {
        return m_config.value("background/enabled", false).toBool();
    }

    void setBackgroundEnabled(bool enabled)
    {
        m_config["background/enabled"] = enabled;
    }

    QString getTranslateApiKey() const
    {
        return m_config.value("translate/apiKey").toString();
    }

    void setTranslateApiKey(const QString& apiKey)
    {
        m_config["translate/apiKey"] = apiKey;
    }

    QString getTranslateSystemPrompt() const
    {
        return m_config.value("translate/systemPrompt").toString();
    }

    void setTranslateSystemPrompt(const QString& prompt)
    {
        m_config["translate/systemPrompt"] = prompt;
    }

    QString getTranslateBatchPrompt() const
    {
        return m_config.value("translate/batchPrompt").toString();
    }

    void setTranslateBatchPrompt(const QString& prompt)
    {
        m_config["translate/batchPrompt"] = prompt;
    }

    int getTranslateBatchSize() const
    {
        return m_config.value("translate/batchSize", 32).toInt();
    }

    void setTranslateBatchSize(int size)
    {
        m_config["translate/batchSize"] = size;
    }

    int getTranslateMaxConcurrent() const
    {
        return m_config.value("translate/maxConcurrent", 3).toInt();
    }

    void setTranslateMaxConcurrent(int count)
    {
        m_config["translate/maxConcurrent"] = count;
    }

    int getTranslateMaxRetries() const
    {
        return m_config.value("translate/maxRetries", 3).toInt();
    }

    void setTranslateMaxRetries(int retries)
    {
        m_config["translate/maxRetries"] = retries;
    }

    QString getTranslateModel() const
    {
        return m_config.value("translate/model", "mimo-v2-flash").toString();
    }

    void setTranslateModel(const QString& model)
    {
        m_config["translate/model"] = model;
    }

    QString getTranslateProvider() const
    {
        return m_config.value("translate/provider", "xiaomi").toString();
    }

    void setTranslateProvider(const QString& provider)
    {
        m_config["translate/provider"] = provider;
    }

    int getFontSize() const
    {
        return m_config.value("general/fontSize", 11).toInt();
    }

    void setFontSize(int size)
    {
        m_config["general/fontSize"] = size;
    }

    bool isReadOnly() const
    {
        return m_config.value("general/readOnly", true).toBool();
    }

    void setReadOnly(bool readOnly)
    {
        m_config["general/readOnly"] = readOnly;
    }

    bool isAutoSaveEnabled() const
    {
        return m_config.value("general/autoSaveEnabled", false).toBool();
    }

    void setAutoSaveEnabled(bool enabled)
    {
        m_config["general/autoSaveEnabled"] = enabled;
    }

    QString getLastOpenPath() const
    {
        return m_config.value("general/lastOpenPath").toString();
    }

    void setLastOpenPath(const QString& path)
    {
        m_config["general/lastOpenPath"] = path;
    }

    static QByteArray encryptData(const QByteArray& data)
    {
        QByteArray result = data;
        const unsigned char* key = ENCRYPTION_KEY;
        int keyLen = sizeof(ENCRYPTION_KEY);
        
        for (int i = 0; i < result.size(); ++i) {
            result[i] = result[i] ^ key[i % keyLen];
            result[i] = static_cast<char>((result[i] + i + key[i % keyLen]) & 0xFF);
        }
        
        return result;
    }

    static QByteArray decryptData(const QByteArray& data)
    {
        QByteArray result = data;
        const unsigned char* key = ENCRYPTION_KEY;
        int keyLen = sizeof(ENCRYPTION_KEY);
        
        for (int i = 0; i < result.size(); ++i) {
            result[i] = static_cast<char>((result[i] - i - key[i % keyLen]) & 0xFF);
            result[i] = result[i] ^ key[i % keyLen];
        }
        
        return result;
    }

private:
    static const quint32 CONFIG_MAGIC = 0x47585443;
    static const quint32 CONFIG_VERSION = 1;
    static constexpr unsigned char ENCRYPTION_KEY[] = {
        0x47, 0x58, 0x54, 0x53, 0x74, 0x75, 0x64, 0x69,
        0x6F, 0x5F, 0x43, 0x6F, 0x6E, 0x66, 0x69, 0x67
    };

    AppConfig() 
    {
        load();
    }
    
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    void migrateFromQSettings()
    {
        qDebug() << "Migrating config from QSettings...";
        
        QSettings settings;
        
        m_config["general/fontSize"] = settings.value("fontSize", 11).toInt();
        m_config["general/readOnly"] = settings.value("readOnly", true).toBool();
        m_config["general/autoSaveEnabled"] = settings.value("autoSaveEnabled", false).toBool();
        
        m_config["background/blurRadius"] = settings.value("background/blurRadius", 0).toInt();
        m_config["background/brightness"] = settings.value("background/brightness", 0).toInt();
        m_config["background/opacity"] = settings.value("background/opacity", 1.0).toDouble();
        m_config["background/enabled"] = settings.value("background/enabled", false).toBool();
        
        settings.beginGroup("Translate");
        m_config["translate/apiKey"] = settings.value("apiKey").toString();
        m_config["translate/systemPrompt"] = settings.value("systemPrompt").toString();
        m_config["translate/batchPrompt"] = settings.value("batchPrompt").toString();
        m_config["translate/batchSize"] = settings.value("batchSize", 32).toInt();
        m_config["translate/maxConcurrent"] = settings.value("maxConcurrent", 3).toInt();
        m_config["translate/maxRetries"] = settings.value("maxRetries", 3).toInt();
        settings.endGroup();
        
        save();
        qDebug() << "Migration completed";
    }

    QVariantMap m_config;
};
