#include "CharTableExportWorker.h"
#include <QPainter>
#include <QElapsedTimer>
#include <QDebug>

CharTableExportWorker::CharTableExportWorker(QObject* parent)
    : QObject(parent)
    , m_cancelRequested(false)
{
    qRegisterMetaType<CharTableExportTask>("CharTableExportTask");
    qRegisterMetaType<CharTableExportResult>("CharTableExportResult");
}

CharTableExportWorker::~CharTableExportWorker()
{
}

void CharTableExportWorker::requestCancel()
{
    m_cancelRequested = true;
}

void CharTableExportWorker::exportImage(const CharTableExportTask& task)
{
    QElapsedTimer timer;
    timer.start();

    CharTableExportResult result;
    result.outputPath = task.outputPath;
    result.success = false;

    m_cancelRequested = false;

    emit progressChanged(5, "正在初始化...");

    QImage image(task.imgSize, task.imgSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    emit progressChanged(10, "正在创建画布...");

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    painter.setFont(task.font);

    QColor textColor(255, 255, 255, 230);
    painter.setPen(textColor);

    int yOffset = (task.type == 1) ? -2 : -2;

    int totalChars = task.characters.size();
    int charIndex = 0;
    int lastProgress = 10;

    for (int row = 0; row < task.rows && charIndex < totalChars; ++row) {
        if (m_cancelRequested) {
            result.success = false;
            result.errorMessage = "用户取消导出";
            result.elapsedMs = timer.elapsed();
            emit exportCompleted(result);
            return;
        }

        for (int col = 0; col < task.cols && charIndex < totalChars; ++col) {
            int x = col * task.cellWidth;
            int y = row * task.cellHeight + yOffset;

            QRect cellRect(x, y, task.cellWidth, task.cellHeight);
            painter.drawText(cellRect, Qt::AlignCenter, QString(task.characters[charIndex]));
            ++charIndex;
        }

        int progress = 10 + (charIndex * 85) / totalChars;
        if (progress > lastProgress + 5 || progress >= 95) {
            lastProgress = progress;
            emit progressChanged(progress, QString("正在渲染字符 %1/%2...").arg(charIndex).arg(totalChars));
        }
    }

    painter.end();

    emit progressChanged(95, "正在保存图片...");

    if (m_cancelRequested) {
        result.success = false;
        result.errorMessage = "用户取消导出";
        result.elapsedMs = timer.elapsed();
        emit exportCompleted(result);
        return;
    }

    if (image.save(task.outputPath, task.format.toUpper().toUtf8().constData())) {
        result.success = true;
        result.elapsedMs = timer.elapsed();
        emit progressChanged(100, "导出完成");
    } else {
        result.success = false;
        result.errorMessage = "无法保存图片到: " + task.outputPath;
        result.elapsedMs = timer.elapsed();
    }

    emit exportCompleted(result);
}
