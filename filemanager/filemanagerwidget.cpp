#include "filemanagerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>

FileManagerWidget::FileManagerWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("文件管理器");

    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf");
    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    QFont titleFont("WenQuanYi Zen Hei", 16, QFont::Bold);
    QFont btnFont("WenQuanYi Zen Hei", 14, QFont::Bold);
    QFont listFont("WenQuanYi Zen Hei", 12);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setFont(QFont("WenQuanYi Zen Hei", 12, QFont::Bold));
    m_pathLabel->setStyleSheet("color: white; background: #2a2a3e; padding: 8px 12px; border-radius: 4px;");
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setMinimumHeight(36);

    m_upBtn = new QPushButton("上一级", this);
    m_upBtn->setFixedSize(100, 40);
    m_upBtn->setFont(btnFont);
    m_upBtn->setStyleSheet("QPushButton { background: #555; color: white; border: none; border-radius: 4px; }"
                           "QPushButton:pressed { background: #333; }");

    m_refreshBtn = new QPushButton("刷新", this);
    m_refreshBtn->setFixedSize(100, 40);
    m_refreshBtn->setFont(btnFont);
    m_refreshBtn->setStyleSheet("QPushButton { background: #2196F3; color: white; border: none; border-radius: 4px; }"
                                "QPushButton:pressed { background: #1565C0; }");

    m_exitBtn = new QPushButton("退出", this);
    m_exitBtn->setFixedSize(100, 40);
    m_exitBtn->setFont(btnFont);
    m_exitBtn->setStyleSheet("QPushButton { background: white; color: #333; border: none; border-radius: 4px; }"
                             "QPushButton:pressed { background: #ccc; }");
    connect(m_exitBtn, &QPushButton::clicked, qApp, &QApplication::quit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    btnRow->addStretch();
    btnRow->addWidget(m_upBtn);
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_exitBtn);
    btnRow->addStretch();

    m_list = new QListWidget(this);
    m_list->setFont(listFont);
    m_list->setStyleSheet(
        "QListWidget { background: #1e1e2e; color: #ddd; border: 1px solid #333; border-radius: 4px; }"
        "QListWidget::item { padding: 10px 8px; border-bottom: 1px solid #2a2a3e; }"
        "QListWidget::item:hover { background: #2a2a4e; }"
        "QListWidget::item:selected { background: #3a3a5e; color: white; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(m_pathLabel);
    mainLayout->addLayout(btnRow);
    mainLayout->addWidget(m_list, 1);

    setStyleSheet("background: #12121e;");

    connect(m_upBtn, &QPushButton::clicked, this, &FileManagerWidget::onGoUp);
    connect(m_refreshBtn, &QPushButton::clicked, this, &FileManagerWidget::onRefresh);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &FileManagerWidget::onItemClicked);

    m_currentDir = QDir::home();
    populateList();
}

FileManagerWidget::~FileManagerWidget()
{
}

void FileManagerWidget::populateList()
{
    m_list->clear();
    m_pathLabel->setText(m_currentDir.absolutePath());

    QFileInfoList entries = m_currentDir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &info : entries) 
    {
        QString text;
        if (info.isDir()) 
        {
            text = QString("[目录]  %1").arg(info.fileName());
        } 
        else 
        {
            qint64 size = info.size();
            QString sizeStr;
            if (size < 1024)
                sizeStr = QString("%1 B").arg(size);
            else if (size < 1024 * 1024)
                sizeStr = QString("%1 KB").arg(size / 1024);
            else
                sizeStr = QString("%1 MB").arg(size / (1024 * 1024));

            QString dateStr = info.lastModified().toString("MM-dd hh:mm");
            text = QString("[文件]  %1    %2    %3").arg(info.fileName(), sizeStr, dateStr);
        }
        QListWidgetItem *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, info.absoluteFilePath());
    }
}

void FileManagerWidget::onItemClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    QString fullPath = item->data(Qt::UserRole).toString();
    if (fullPath.isEmpty())
        return;

    QFileInfo info(fullPath);
    if (info.isDir() && info.isReadable()) 
    {
        m_currentDir.setPath(fullPath);
        populateList();
    }
}

void FileManagerWidget::onGoUp()
{
    if (m_currentDir.cdUp())
 {
        populateList();
    }
}

void FileManagerWidget::onRefresh()
{
    populateList();
}