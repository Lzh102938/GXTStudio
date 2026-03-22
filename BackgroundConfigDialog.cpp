#include "BackgroundConfigDialog.h"
#include "FontAwesome.h"
#include <QPainter>
#include <QScrollArea>
#include <QApplication>
#include <QStyle>
#include <algorithm>
#include <cmath>

BackgroundConfigDialog::BackgroundConfigDialog(QWidget* parent)
    : QDialog(parent)
    , m_blurRadius(0)
    , m_brightness(0)
    , m_opacity(100)
    , m_isProcessing(false)
    , m_pendingBlur(0)
    , m_pendingBrightness(0)
{
    setupUI();
    
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(150);
    connect(m_previewTimer, &QTimer::timeout, this, &BackgroundConfigDialog::onPreviewTimerTimeout);
    
    m_processWatcher = new QFutureWatcher<QImage>(this);
    connect(m_processWatcher, &QFutureWatcher<QImage>::finished, this, &BackgroundConfigDialog::onProcessFinished);
}

BackgroundConfigDialog::~BackgroundConfigDialog()
{
    if (m_processWatcher->isRunning()) {
        m_processWatcher->waitForFinished();
    }
}

void BackgroundConfigDialog::setupUI()
{
    setWindowTitle(QString::fromUtf8("背景图片设置"));
    setMinimumSize(600, 500);
    setModal(true);
    
    setStyleSheet(R"(
        QDialog {
            background-color: #f5f7fa;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 13px;
            color: #2c3e50;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 10px;
            background-color: white;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px;
        }
        QSlider::groove:horizontal {
            height: 8px;
            background: #e0e0e0;
            border-radius: 4px;
        }
        QSlider::handle:horizontal {
            width: 20px;
            height: 20px;
            margin: -6px 0;
            background: #5b9fd6;
            border-radius: 10px;
        }
        QSlider::handle:horizontal:hover {
            background: #4a90d9;
        }
        QSlider::sub-page:horizontal {
            background: #5b9fd6;
            border-radius: 4px;
        }
        QLabel {
            color: #333;
        }
        QPushButton {
            padding: 8px 20px;
            border-radius: 6px;
            font-weight: bold;
        }
    )");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    QGroupBox* settingsGroup = new QGroupBox(QString::fromUtf8("效果设置"), this);
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsGroup);
    settingsLayout->setSpacing(20);
    settingsLayout->setContentsMargins(15, 20, 15, 15);
    
    auto createSliderRow = [this, &settingsLayout](const QString& label, const QChar& icon, 
                                                    int min, int max, int defaultValue,
                                                    QSlider*& slider, QLabel*& valueLabel) {
        QHBoxLayout* rowLayout = new QHBoxLayout();
        rowLayout->setSpacing(10);
        
        QLabel* iconLabel = new QLabel(icon, this);
        iconLabel->setFont(FA::solidFont(14));
        iconLabel->setStyleSheet("color: #5b9fd6;");
        iconLabel->setFixedWidth(24);
        
        QLabel* textLabel = new QLabel(label, this);
        textLabel->setMinimumWidth(60);
        textLabel->setStyleSheet("font-size: 13px;");
        
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(min, max);
        slider->setValue(defaultValue);
        slider->setMinimumWidth(200);
        
        valueLabel = new QLabel(QString::number(defaultValue), this);
        valueLabel->setMinimumWidth(40);
        valueLabel->setAlignment(Qt::AlignCenter);
        valueLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #5b9fd6;");
        
        rowLayout->addWidget(iconLabel);
        rowLayout->addWidget(textLabel);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(valueLabel);
        
        settingsLayout->addLayout(rowLayout);
    };
    
    createSliderRow(QString::fromUtf8("高斯模糊"), FA::QPalette, 0, 50, 0, m_blurSlider, m_blurValueLabel);
    createSliderRow(QString::fromUtf8("亮度调节"), FA::QLightbulb, -100, 100, 0, m_brightnessSlider, m_brightnessValueLabel);
    createSliderRow(QString::fromUtf8("不透明度"), FA::QCog, 10, 100, 100, m_opacitySlider, m_opacityValueLabel);
    
    mainLayout->addWidget(settingsGroup);
    
    QGroupBox* previewGroup = new QGroupBox(QString::fromUtf8("预览"), this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setContentsMargins(10, 15, 10, 10);
    
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumHeight(200);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: #e8e8e8; border-radius: 8px; color: #999;");
    m_previewLabel->setText(QString::fromUtf8("请先选择背景图片"));
    m_previewLabel->setScaledContents(false);
    
    previewLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup, 1);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();
    
    m_resetButton = new QPushButton(QString::fromUtf8("重置"), this);
    m_resetButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: 1px solid #d0d0d0;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
    )");
    
    m_cancelButton = new QPushButton(QString::fromUtf8("取消"), this);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: 1px solid #d0d0d0;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
    )");
    
    m_applyButton = new QPushButton(QString::fromUtf8("应用"), this);
    m_applyButton->setStyleSheet(R"(
        QPushButton {
            background-color: #5b9fd6;
            color: white;
            border: none;
        }
        QPushButton:hover {
            background-color: #4a90d9;
        }
    )");
    
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_applyButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(m_blurSlider, &QSlider::valueChanged, this, &BackgroundConfigDialog::onBlurChanged);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &BackgroundConfigDialog::onBrightnessChanged);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &BackgroundConfigDialog::onOpacityChanged);
    connect(m_applyButton, &QPushButton::clicked, this, &BackgroundConfigDialog::onApplyClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &BackgroundConfigDialog::onResetClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void BackgroundConfigDialog::setBackgroundImage(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        m_originalPixmap = QPixmap();
        m_previewLabel->setText(QString::fromUtf8("请先选择背景图片"));
        m_previewLabel->setPixmap(QPixmap());
        return;
    }

    m_originalPixmap = pixmap;
    updatePreview();
}

void BackgroundConfigDialog::setBlurRadius(int radius)
{
    m_blurRadius = radius;
    m_blurSlider->blockSignals(true);
    m_blurSlider->setValue(radius);
    m_blurSlider->blockSignals(false);
    m_blurValueLabel->setText(QString::number(radius));
}

void BackgroundConfigDialog::setBrightness(int brightness)
{
    m_brightness = brightness;
    m_brightnessSlider->blockSignals(true);
    m_brightnessSlider->setValue(brightness);
    m_brightnessSlider->blockSignals(false);
    m_brightnessValueLabel->setText(QString::number(brightness));
}

void BackgroundConfigDialog::setOpacity(int opacity)
{
    m_opacity = opacity;
    m_opacitySlider->blockSignals(true);
    m_opacitySlider->setValue(opacity);
    m_opacitySlider->blockSignals(false);
    m_opacityValueLabel->setText(QString::number(opacity));
}

QPixmap BackgroundConfigDialog::getProcessedImage() const
{
    if (m_blurRadius == 0 && m_brightness == 0) {
        return m_originalPixmap;
    }
    return m_processedPixmap;
}

void BackgroundConfigDialog::onBlurChanged(int value)
{
    m_blurRadius = value;
    m_blurValueLabel->setText(QString::number(value));
    m_previewTimer->start();
}

void BackgroundConfigDialog::onBrightnessChanged(int value)
{
    m_brightness = value;
    m_brightnessValueLabel->setText(QString::number(value));
    m_previewTimer->start();
}

void BackgroundConfigDialog::onOpacityChanged(int value)
{
    m_opacity = value;
    m_opacityValueLabel->setText(QString::number(value));
    m_previewTimer->start();
}

void BackgroundConfigDialog::onApplyClicked()
{
    emit settingsChanged(m_blurRadius, m_brightness, m_opacity);
    accept();
}

void BackgroundConfigDialog::onResetClicked()
{
    setBlurRadius(0);
    setBrightness(0);
    setOpacity(100);
    updatePreview();
}

void BackgroundConfigDialog::onPreviewTimerTimeout()
{
    if (m_originalPixmap.isNull()) {
        return;
    }
    
    m_pendingBlur = m_blurRadius;
    m_pendingBrightness = m_brightness;
    
    startAsyncProcess();
}

void BackgroundConfigDialog::startAsyncProcess()
{
    if (m_isProcessing) {
        return;
    }
    
    m_isProcessing = true;
    m_applyButton->setEnabled(false);
    m_previewLabel->setText(QString::fromUtf8("处理中..."));
    
    QImage sourceImage = m_originalPixmap.toImage();
    int blur = m_pendingBlur;
    int brightness = m_pendingBrightness;
    
    QFuture<QImage> future = QtConcurrent::run([this, sourceImage, blur, brightness]() {
        return applyEffects(sourceImage, blur, brightness);
    });
    
    m_processWatcher->setFuture(future);
}

void BackgroundConfigDialog::onProcessFinished()
{
    QImage result = m_processWatcher->result();
    
    if (!result.isNull()) {
        m_processedPixmap = QPixmap::fromImage(result);
        
        QSize labelSize = m_previewLabel->size();
        QPixmap scaledPixmap = m_processedPixmap.scaled(
            labelSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        
        QPixmap displayPixmap(scaledPixmap.size());
        displayPixmap.fill(Qt::transparent);
        
        QPainter painter(&displayPixmap);
        painter.setOpacity(m_opacity / 100.0);
        painter.drawPixmap(0, 0, scaledPixmap);
        painter.end();
        
        m_previewLabel->setPixmap(displayPixmap);
    }
    
    m_isProcessing = false;
    m_applyButton->setEnabled(true);
}

void BackgroundConfigDialog::updatePreview()
{
    if (m_originalPixmap.isNull()) {
        return;
    }
    
    m_pendingBlur = m_blurRadius;
    m_pendingBrightness = m_brightness;
    startAsyncProcess();
}

QImage BackgroundConfigDialog::applyEffects(const QImage& source, int blurRadius, int brightness)
{
    QImage result = source;
    
    if (blurRadius > 0) {
        result = applyGaussianBlur(result, blurRadius);
    }
    
    if (brightness != 0) {
        result = applyBrightness(result, brightness);
    }
    
    return result;
}

QImage BackgroundConfigDialog::applyGaussianBlur(const QImage& source, int radius)
{
    if (radius <= 0 || source.isNull()) {
        return source;
    }
    
    int optimizedRadius = qMin(radius, 25);
    
    QImage temp = source.convertToFormat(QImage::Format_ARGB32);
    QImage result(temp.size(), QImage::Format_ARGB32);
    
    int w = temp.width();
    int h = temp.height();
    
    int kernelSize = optimizedRadius * 2 + 1;
    QVector<float> kernel(kernelSize);
    float sigma = optimizedRadius / 3.0f;
    float sum = 0.0f;
    
    for (int i = 0; i < kernelSize; ++i) {
        float x = i - optimizedRadius;
        kernel[i] = std::exp(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < kernelSize; ++i) {
        kernel[i] /= sum;
    }
    
    QImage intermediate(w, h, QImage::Format_ARGB32);
    
    const QRgb* srcBits = reinterpret_cast<const QRgb*>(temp.constBits());
    QRgb* dstBits = reinterpret_cast<QRgb*>(intermediate.bits());
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            
            for (int k = 0; k < kernelSize; ++k) {
                int px = qBound(0, x + k - optimizedRadius, w - 1);
                QRgb pixel = srcBits[y * w + px];
                float weight = kernel[k];
                r += qRed(pixel) * weight;
                g += qGreen(pixel) * weight;
                b += qBlue(pixel) * weight;
                a += qAlpha(pixel) * weight;
            }
            
            dstBits[y * w + x] = qRgba(
                static_cast<int>(r + 0.5f),
                static_cast<int>(g + 0.5f),
                static_cast<int>(b + 0.5f),
                static_cast<int>(a + 0.5f)
            );
        }
    }
    
    srcBits = reinterpret_cast<const QRgb*>(intermediate.constBits());
    dstBits = reinterpret_cast<QRgb*>(result.bits());
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            
            for (int k = 0; k < kernelSize; ++k) {
                int py = qBound(0, y + k - optimizedRadius, h - 1);
                QRgb pixel = srcBits[py * w + x];
                float weight = kernel[k];
                r += qRed(pixel) * weight;
                g += qGreen(pixel) * weight;
                b += qBlue(pixel) * weight;
                a += qAlpha(pixel) * weight;
            }
            
            dstBits[y * w + x] = qRgba(
                static_cast<int>(r + 0.5f),
                static_cast<int>(g + 0.5f),
                static_cast<int>(b + 0.5f),
                static_cast<int>(a + 0.5f)
            );
        }
    }
    
    return result;
}

QImage BackgroundConfigDialog::applyBrightness(const QImage& source, int brightness)
{
    if (brightness == 0 || source.isNull()) {
        return source;
    }
    
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    
    QRgb* bits = reinterpret_cast<QRgb*>(result.bits());
    int pixelCount = result.width() * result.height();
    
    for (int i = 0; i < pixelCount; ++i) {
        QRgb pixel = bits[i];
        int r = qBound(0, qRed(pixel) + brightness, 255);
        int g = qBound(0, qGreen(pixel) + brightness, 255);
        int b = qBound(0, qBlue(pixel) + brightness, 255);
        bits[i] = qRgba(r, g, b, qAlpha(pixel));
    }
    
    return result;
}
