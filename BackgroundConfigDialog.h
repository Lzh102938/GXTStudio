#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QGroupBox>
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent>

class BackgroundConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BackgroundConfigDialog(QWidget* parent = nullptr);
    ~BackgroundConfigDialog();

    void setBackgroundImage(const QPixmap& pixmap);
    QPixmap getProcessedImage() const;
    
    int getBlurRadius() const { return m_blurRadius; }
    int getBrightness() const { return m_brightness; }
    int getOpacity() const { return m_opacity; }
    
    void setBlurRadius(int radius);
    void setBrightness(int brightness);
    void setOpacity(int opacity);

signals:
    void settingsChanged(int blurRadius, int brightness, int opacity);

private slots:
    void onBlurChanged(int value);
    void onBrightnessChanged(int value);
    void onOpacityChanged(int value);
    void onApplyClicked();
    void onResetClicked();
    void onPreviewTimerTimeout();
    void onProcessFinished();

private:
    void setupUI();
    void updatePreview();
    void startAsyncProcess();
    QImage applyEffects(const QImage& source, int blurRadius, int brightness);
    QImage applyGaussianBlur(const QImage& source, int radius);
    QImage applyBrightness(const QImage& source, int brightness);

private:
    QPixmap m_originalPixmap;
    QPixmap m_processedPixmap;
    
    QSlider* m_blurSlider;
    QSlider* m_brightnessSlider;
    QSlider* m_opacitySlider;
    
    QLabel* m_blurValueLabel;
    QLabel* m_brightnessValueLabel;
    QLabel* m_opacityValueLabel;
    QLabel* m_previewLabel;
    
    QPushButton* m_applyButton;
    QPushButton* m_resetButton;
    QPushButton* m_cancelButton;
    
    int m_blurRadius;
    int m_brightness;
    int m_opacity;
    
    QTimer* m_previewTimer;
    QFutureWatcher<QImage>* m_processWatcher;
    bool m_isProcessing;
    
    int m_pendingBlur;
    int m_pendingBrightness;
};
