#ifndef REPLACEDIALOG_H
#define REPLACEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QButtonGroup>

class ReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    enum Operation {
        FindNext,
        Replace,
        ReplaceAll,
        None
    };

public:
    explicit ReplaceDialog(QWidget *parent = nullptr);
    ~ReplaceDialog();
    
    Operation getOperation() const { return m_operation; }

    // 获取用户选择的替换范围
    enum ReplaceScope {
        CurrentTable,
        AllTables,
        SelectedArea
    };
    
    ReplaceScope getReplaceScope() const;
    bool isCaseSensitive() const;
    bool isLoopEnabled() const;
    void setLoopEnabled(bool enabled);
    bool isRegexEnabled() const;
    void setRegexEnabled(bool enabled);
    void setCaseSensitive(bool enabled);
    QString findText() const;
    QString replaceText() const;




signals:
    void findNextClicked(const QString& findText, bool caseSensitive);
    void replaceClicked(const QString& findText, const QString& replaceText, bool caseSensitive, ReplaceScope scope);
    void replaceAllClicked(const QString& findText, const QString& replaceText, bool caseSensitive, ReplaceScope scope);
    void caseSensitiveToggled(bool enabled);
    void regexToggled(bool enabled);
    void loopToggled(bool enabled);



private slots:
    void onFindNext();
    void onReplace();
    void onReplaceAll();
    void onTextChanged();

public:
    QLineEdit* findEdit() const { return m_findEdit; }
    QLineEdit* replaceEdit() const { return m_replaceEdit; }

private:
    QLineEdit* m_findEdit;
    QLineEdit* m_replaceEdit;
    
    QRadioButton* m_currentTableRadio;
    QRadioButton* m_allTablesRadio;
    QRadioButton* m_selectedAreaRadio;
    QButtonGroup* m_scopeGroup;
    Operation m_operation;
    
    QCheckBox* m_caseSensitiveCheck;
    QCheckBox* m_regexCheck;
    QCheckBox* m_loopCheck;
    
    QPushButton* m_findNextButton;

    QPushButton* m_replaceButton;
    QPushButton* m_replaceAllButton;
    QPushButton* m_cancelButton;
    
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_findLayout;
    QHBoxLayout* m_replaceLayout;
    QGroupBox* m_scopeGroupWidget;
    QHBoxLayout* m_scopeLayout;
    QHBoxLayout* m_buttonsLayout;
};

#endif // REPLACEDIALOG_H