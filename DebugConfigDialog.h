#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDataStream>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonObject>
#include "AppConfig.h"

class DebugConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DebugConfigDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QString::fromUtf8("调试配置编辑器"));
        setMinimumSize(700, 500);
        
        setupUI();
        loadConfig();
    }

private slots:
    void onSave()
    {
        QString content = m_textEdit->toPlainText();
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8(), &error);
        
        if (error.error != QJsonParseError::NoError) {
            QMessageBox::warning(this, QString::fromUtf8("JSON格式错误"),
                QString::fromUtf8("JSON解析错误: %1\n位置: %2")
                .arg(error.errorString()).arg(error.offset));
            return;
        }
        
        QString configPath = getConfigFilePath();
        
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        
        out << static_cast<quint32>(0x47585443);
        out << static_cast<quint32>(1);
        
        QVariantMap config = doc.object().toVariantMap();
        out << config;
        
        QByteArray encryptedData = AppConfig::encryptData(data);
        
        QFile file(configPath);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, QString::fromUtf8("保存失败"),
                QString::fromUtf8("无法打开配置文件: %1").arg(configPath));
            return;
        }
        
        file.write(encryptedData);
        file.close();
        
        QMessageBox::information(this, QString::fromUtf8("保存成功"),
            QString::fromUtf8("配置已保存到: %1\n\n请重启程序以应用更改。").arg(configPath));
    }

    void onReload()
    {
        loadConfig();
    }

    void onExportJson()
    {
        QString filePath = QFileDialog::getSaveFileName(this,
            QString::fromUtf8("导出配置"),
            QString(),
            "JSON文件 (*.json)");
        
        if (filePath.isEmpty()) {
            return;
        }
        
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, QString::fromUtf8("导出失败"),
                QString::fromUtf8("无法创建文件: %1").arg(filePath));
            return;
        }
        
        file.write(m_textEdit->toPlainText().toUtf8());
        file.close();
        
        QMessageBox::information(this, QString::fromUtf8("导出成功"),
            QString::fromUtf8("配置已导出到: %1").arg(filePath));
    }

    void onImportJson()
    {
        QString filePath = QFileDialog::getOpenFileName(this,
            QString::fromUtf8("导入配置"),
            QString(),
            "JSON文件 (*.json)");
        
        if (filePath.isEmpty()) {
            return;
        }
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, QString::fromUtf8("导入失败"),
                QString::fromUtf8("无法打开文件: %1").arg(filePath));
            return;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonParseError error;
        QJsonDocument::fromJson(data, &error);
        
        if (error.error != QJsonParseError::NoError) {
            QMessageBox::warning(this, QString::fromUtf8("JSON格式错误"),
                QString::fromUtf8("JSON解析错误: %1").arg(error.errorString()));
            return;
        }
        
        m_textEdit->setPlainText(QString::fromUtf8(data));
    }

private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(15, 15, 15, 15);
        mainLayout->setSpacing(10);
        
        QLabel* pathLabel = new QLabel(this);
        pathLabel->setText(QString::fromUtf8("配置文件: %1").arg(getConfigFilePath()));
        pathLabel->setStyleSheet("color: #666; font-size: 11px;");
        mainLayout->addWidget(pathLabel);
        
        m_textEdit = new QTextEdit(this);
        m_textEdit->setAcceptRichText(false);
        m_textEdit->setStyleSheet(
            "QTextEdit {"
            "  font-family: Consolas, 'Courier New', monospace;"
            "  font-size: 12px;"
            "  background-color: #1e1e1e;"
            "  color: #d4d4d4;"
            "  border: 1px solid #3c3c3c;"
            "  border-radius: 4px;"
            "}"
        );
        mainLayout->addWidget(m_textEdit);
        
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->setSpacing(10);
        
        QPushButton* importButton = new QPushButton(QString::fromUtf8("导入JSON"), this);
        importButton->setFixedWidth(100);
        connect(importButton, &QPushButton::clicked, this, &DebugConfigDialog::onImportJson);
        buttonLayout->addWidget(importButton);
        
        QPushButton* exportButton = new QPushButton(QString::fromUtf8("导出JSON"), this);
        exportButton->setFixedWidth(100);
        connect(exportButton, &QPushButton::clicked, this, &DebugConfigDialog::onExportJson);
        buttonLayout->addWidget(exportButton);
        
        buttonLayout->addStretch();
        
        QPushButton* reloadButton = new QPushButton(QString::fromUtf8("重新加载"), this);
        reloadButton->setFixedWidth(100);
        connect(reloadButton, &QPushButton::clicked, this, &DebugConfigDialog::onReload);
        buttonLayout->addWidget(reloadButton);
        
        QPushButton* saveButton = new QPushButton(QString::fromUtf8("保存"), this);
        saveButton->setFixedWidth(80);
        connect(saveButton, &QPushButton::clicked, this, &DebugConfigDialog::onSave);
        buttonLayout->addWidget(saveButton);
        
        QPushButton* closeButton = new QPushButton(QString::fromUtf8("关闭"), this);
        closeButton->setFixedWidth(80);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addLayout(buttonLayout);
        
        QLabel* tipLabel = new QLabel(QString::fromUtf8("提示: 编辑JSON格式配置，保存后重启程序生效"), this);
        tipLabel->setStyleSheet("color: #888; font-size: 10px;");
        mainLayout->addWidget(tipLabel);
    }

    void loadConfig()
    {
        QString configPath = getConfigFilePath();
        QFile file(configPath);
        
        if (!file.exists()) {
            m_textEdit->setPlainText(QString::fromUtf8("{\n  \"general\": {},\n  \"background\": {},\n  \"translate\": {}\n}"));
            return;
        }
        
        if (!file.open(QIODevice::ReadOnly)) {
            m_textEdit->setPlainText(QString::fromUtf8("// 无法打开配置文件"));
            return;
        }
        
        QByteArray encryptedData = file.readAll();
        file.close();
        
        QByteArray decryptedData = AppConfig::decryptData(encryptedData);
        
        QDataStream in(&decryptedData, QIODevice::ReadOnly);
        in.setVersion(QDataStream::Qt_6_0);
        
        quint32 magic;
        in >> magic;
        
        if (magic != 0x47585443) {
            m_textEdit->setPlainText(QString::fromUtf8("// 无效的配置文件格式"));
            return;
        }
        
        quint32 version;
        in >> version;
        
        QVariantMap config;
        in >> config;
        
        QJsonDocument doc = QJsonDocument::fromVariant(config);
        QString jsonStr = doc.toJson(QJsonDocument::Indented);
        
        m_textEdit->setPlainText(jsonStr);
    }

    QString getConfigFilePath() const
    {
        QString appDir = QCoreApplication::applicationDirPath();
        return appDir + "/config.dat";
    }

    QTextEdit* m_textEdit;
};
