#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QFont>
#include <QImage>
#include <atomic>
#include <QMetaType>

struct CharTableExportTask {
    QString outputPath;
    QString format;
    QFont font;
    QVector<QChar> characters;
    int type;
    int rows;
    int cols;
    int cellWidth;
    int cellHeight;
    int imgSize;
};

struct CharTableExportResult {
    bool success;
    QString outputPath;
    QString errorMessage;
    qint64 elapsedMs;
};

Q_DECLARE_METATYPE(CharTableExportTask)
Q_DECLARE_METATYPE(CharTableExportResult)

class CharTableExportWorker : public QObject {
    Q_OBJECT

public:
    explicit CharTableExportWorker(QObject* parent = nullptr);
    ~CharTableExportWorker();

    void requestCancel();

public slots:
    void exportImage(const CharTableExportTask& task);

signals:
    void progressChanged(int percentage, const QString& message);
    void exportCompleted(const CharTableExportResult& result);

private:
    std::atomic<bool> m_cancelRequested;
};
