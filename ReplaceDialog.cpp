#include "ReplaceDialog.h"
#include <QLabel>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QButtonGroup>
#include <QPushButton>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>

ReplaceDialog::ReplaceDialog(QWidget *parent)
    : QDialog(parent), m_operation(None)
{
    setWindowTitle("替换");
    setModal(false);  // 非模态，对父窗口前置由 raise/activate 保证
    
    // 只在主窗口前置，去除全局置顶
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);
    

    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // 查找内容区域
    QLabel* findLabel = new QLabel("查找内容(&N):");
    findLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    m_findEdit = new QLineEdit();
    m_findEdit->setPlaceholderText("输入要查找的文本...");
    m_findEdit->setMaximumHeight(28);
    
    m_findLayout = new QHBoxLayout();
    m_findLayout->addWidget(findLabel);
    m_findLayout->addWidget(m_findEdit);
    m_findLayout->setSpacing(8);
    
    // 替换为区域
    QLabel* replaceLabel = new QLabel("替换为(&R):");
    replaceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    m_replaceEdit = new QLineEdit();
    m_replaceEdit->setPlaceholderText("输入替换文本...");
    m_replaceEdit->setMaximumHeight(28);
    
    m_replaceLayout = new QHBoxLayout();
    m_replaceLayout->addWidget(replaceLabel);
    m_replaceLayout->addWidget(m_replaceEdit);
    m_replaceLayout->setSpacing(8);
    
    // 作用范围区域
    m_scopeGroupWidget = new QGroupBox("作用范围");
    m_scopeGroupWidget->setCheckable(false);
    
    m_currentTableRadio = new QRadioButton("仅为本表替换");
    m_allTablesRadio = new QRadioButton("为所有表替换");
    m_selectedAreaRadio = new QRadioButton("为选定区域替换");
    
    // 设置默认选中"仅为本表替换"
    m_currentTableRadio->setChecked(true);
    
    m_scopeGroup = new QButtonGroup(this);
    m_scopeGroup->addButton(m_currentTableRadio, static_cast<int>(CurrentTable));
    m_scopeGroup->addButton(m_allTablesRadio, static_cast<int>(AllTables));
    m_scopeGroup->addButton(m_selectedAreaRadio, static_cast<int>(SelectedArea));
    
    m_scopeLayout = new QHBoxLayout();
    m_scopeLayout->addWidget(m_currentTableRadio);
    m_scopeLayout->addWidget(m_allTablesRadio);
    m_scopeLayout->addWidget(m_selectedAreaRadio);
    m_scopeLayout->setSpacing(15);
    
    m_scopeGroupWidget->setLayout(m_scopeLayout);
    
    // 选项区域
    m_caseSensitiveCheck = new QCheckBox("区分大小写(&C)");
    m_regexCheck = new QCheckBox("正则表达式(&R)");
    m_loopCheck = new QCheckBox("循环(&L)");
    
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    optionsLayout->addWidget(m_caseSensitiveCheck);
    optionsLayout->addWidget(m_regexCheck);
    optionsLayout->addWidget(m_loopCheck);

    optionsLayout->addStretch();
    optionsLayout->setSpacing(10);
    
    // 按钮区域
    m_findNextButton = new QPushButton("查找下一个(&F)");
    m_replaceButton = new QPushButton("替换(&T)");
    m_replaceAllButton = new QPushButton("全部替换(&A)");
    m_cancelButton = new QPushButton("取消(&U)");
    
    m_buttonsLayout = new QHBoxLayout();
    m_buttonsLayout->addWidget(m_findNextButton);
    m_buttonsLayout->addWidget(m_replaceButton);
    m_buttonsLayout->addWidget(m_replaceAllButton);
    m_buttonsLayout->addStretch();
    m_buttonsLayout->addWidget(m_cancelButton);
    m_buttonsLayout->setSpacing(8);
    
    // 添加到主布局
    m_mainLayout->addLayout(m_findLayout);
    m_mainLayout->addLayout(m_replaceLayout);
    m_mainLayout->addWidget(m_scopeGroupWidget);
    m_mainLayout->addLayout(optionsLayout);
    m_mainLayout->addLayout(m_buttonsLayout);
    
    // 连接信号槽
    connect(m_findEdit, &QLineEdit::textChanged, this, &ReplaceDialog::onTextChanged);
    connect(m_replaceEdit, &QLineEdit::textChanged, this, &ReplaceDialog::onTextChanged);
    
    connect(m_findNextButton, &QPushButton::clicked, this, &ReplaceDialog::onFindNext);
    connect(m_replaceButton, &QPushButton::clicked, this, &ReplaceDialog::onReplace);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &ReplaceDialog::onReplaceAll);
    connect(m_cancelButton, &QPushButton::clicked, this, &ReplaceDialog::reject);

    connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &ReplaceDialog::caseSensitiveToggled);
    connect(m_regexCheck, &QCheckBox::toggled, this, &ReplaceDialog::regexToggled);
    connect(m_loopCheck, &QCheckBox::toggled, this, &ReplaceDialog::loopToggled);
    
    // 设置按钮快捷键


    m_findNextButton->setShortcut(QKeySequence("F3"));
    m_replaceButton->setShortcut(QKeySequence("Ctrl+H"));
    m_replaceAllButton->setShortcut(QKeySequence("Ctrl+Alt+H"));
    m_cancelButton->setShortcut(QKeySequence("Esc"));
    
    // 设置窗口大小
    setFixedSize(400, 280);
}

ReplaceDialog::~ReplaceDialog()
{
}

ReplaceDialog::ReplaceScope ReplaceDialog::getReplaceScope() const
{
    int id = m_scopeGroup->checkedId();
    if (id == static_cast<int>(AllTables)) {
        return AllTables;
    } else if (id == static_cast<int>(SelectedArea)) {
        return SelectedArea;
    }
    return CurrentTable;
}

bool ReplaceDialog::isCaseSensitive() const
{
    return m_caseSensitiveCheck->isChecked();
}

bool ReplaceDialog::isLoopEnabled() const
{
    return m_loopCheck->isChecked();
}

void ReplaceDialog::setLoopEnabled(bool enabled)
{
    m_loopCheck->setChecked(enabled);
}

bool ReplaceDialog::isRegexEnabled() const
{
    return m_regexCheck->isChecked();
}

void ReplaceDialog::setRegexEnabled(bool enabled)
{
    m_regexCheck->setChecked(enabled);
}

void ReplaceDialog::setCaseSensitive(bool enabled)
{
    m_caseSensitiveCheck->setChecked(enabled);
}


QString ReplaceDialog::findText() const

{
    return m_findEdit->text().trimmed();
}


QString ReplaceDialog::replaceText() const
{
    return m_replaceEdit->text();
}

// 槽函数实现
void ReplaceDialog::onFindNext()
{
    m_operation = FindNext;
    emit findNextClicked(findText(), m_caseSensitiveCheck->isChecked());
}

void ReplaceDialog::onReplace()
{
    m_operation = Replace;
    emit replaceClicked(findText(), replaceText(), m_caseSensitiveCheck->isChecked(), 
                       static_cast<ReplaceScope>(m_scopeGroup->checkedId()));
}

void ReplaceDialog::onReplaceAll()
{
    m_operation = ReplaceAll;
    emit replaceAllClicked(findText(), replaceText(), m_caseSensitiveCheck->isChecked(),
                          static_cast<ReplaceScope>(m_scopeGroup->checkedId()));
}

void ReplaceDialog::onTextChanged()
{
    // 启用/禁用按钮
    bool hasFindText = !m_findEdit->text().trimmed().isEmpty();
    m_findNextButton->setEnabled(hasFindText);
    m_replaceButton->setEnabled(hasFindText);
    m_replaceAllButton->setEnabled(hasFindText);
}