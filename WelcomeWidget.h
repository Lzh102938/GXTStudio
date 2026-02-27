#pragma once

#include <QWidget>

class WelcomeWidget : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget* parent = nullptr);
    ~WelcomeWidget();

signals:
    void openFileRequested();
    void helpRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void initializeUI();
};
