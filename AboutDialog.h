#pragma once

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QEvent>
#include <QSizePolicy>
#include "GXTStudio.h"

class VersionCard : public QFrame {
    Q_OBJECT

public:
    explicit VersionCard(const QString& version, const QString& date, const QString& changes, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void toggleExpand();

private:
    void setupUI();
    void updateArrowIcon();
    QString getVersionCardStyle() const;

    QString m_version;
    QString m_date;
    QString m_changes;
    bool m_expanded;

    QLabel* m_arrowLabel;
    QLabel* m_changesLabel;
    QVBoxLayout* m_contentLayout;
};

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onCheckUpdateClicked();
    void openHomePage();

private:
    QLabel* createInfoLabel(const QString& title, const QString& value);

private:
    void setupUI();
    void createHeaderSection();
    void createInfoAndFileTypeSection();
    void createChangelogSection();
    void createTeamSection();
    void createFooterSection();
    
    QString getCardStyle() const;
    QString getSectionTitleStyle() const;
    QString getButtonStyle() const;

    // 主布局
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    
    // 底部按钮
    QPushButton* m_checkUpdateButton;
};
