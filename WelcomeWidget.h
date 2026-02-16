#pragma once

#include <QWidget>

class WelcomeWidget : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget* parent = nullptr);
    ~WelcomeWidget();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void initializeUI();
};
