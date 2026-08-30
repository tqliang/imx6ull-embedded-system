#include "gallerydialog.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QFile>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QScrollArea>

GalleryDialog::GalleryDialog(const QString &dir, QWidget *parent)
    : QDialog(parent)
    , m_dir(dir)
{
    setWindowTitle("相册");
    setMinimumSize(640, 420);

    QFontDatabase::addApplicationFont("/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc");

    m_stack = new QVBoxLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    m_listPage = new QWidget(this);
    QVBoxLayout *listLayout = new QVBoxLayout(m_listPage);

    m_list = new QListWidget(m_listPage);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(120, 90));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(8);
    m_list->setFont(QFont("WenQuanYi Zen Hei", 10));
    m_list->setStyleSheet("QListWidget { background: #333; border: none; color: #ccc; }"
                          "QListWidget::item { background: #444; border-radius: 4px; padding: 4px; }"
                          "QListWidget::item:selected { background: #4CAF50; }");

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_deleteBtn = new QPushButton("删除", m_listPage);
    m_deleteBtn->setFixedSize(100, 36);
    m_deleteBtn->setFont(QFont("WenQuanYi Zen Hei", 14, QFont::Bold));
    m_deleteBtn->setStyleSheet("QPushButton { background: #f44336; color: white; border: none; border-radius: 4px; }"
                               "QPushButton:pressed { background: #c62828; }");

    m_backBtn = new QPushButton("返回", m_listPage);
    m_backBtn->setFixedSize(100, 36);
    m_backBtn->setFont(QFont("WenQuanYi Zen Hei", 14, QFont::Bold));
    m_backBtn->setStyleSheet("QPushButton { background: #555; color: white; border: none; border-radius: 4px; }"
                             "QPushButton:pressed { background: #333; }");
    connect(m_backBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnRow->addStretch();
    btnRow->addWidget(m_deleteBtn);
    btnRow->addWidget(m_backBtn);
    btnRow->addStretch();

    listLayout->addWidget(m_list);
    listLayout->addLayout(btnRow);

    m_previewPage = new QWidget(this);
    QVBoxLayout *previewLayout = new QVBoxLayout(m_previewPage);

    m_preview = new QLabel(m_previewPage);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(640, 360);
    m_preview->setStyleSheet("background: #111;");

    QPushButton *closePreviewBtn = new QPushButton("返回列表", m_previewPage);
    closePreviewBtn->setFixedSize(120, 36);
    closePreviewBtn->setFont(QFont("WenQuanYi Zen Hei", 14, QFont::Bold));
    closePreviewBtn->setStyleSheet("QPushButton { background: #555; color: white; border: none; border-radius: 4px; }"
                                   "QPushButton:pressed { background: #333; }");
    connect(closePreviewBtn, &QPushButton::clicked, this, &GalleryDialog::onBack);

    QHBoxLayout *previewBtnRow = new QHBoxLayout;
    previewBtnRow->addStretch();
    previewBtnRow->addWidget(closePreviewBtn);
    previewBtnRow->addStretch();

    previewLayout->addWidget(m_preview);
    previewLayout->addLayout(previewBtnRow);

    m_stack->addWidget(m_listPage);
    m_stack->addWidget(m_previewPage);
    m_previewPage->hide();

    connect(m_list, &QListWidget::itemClicked, this, &GalleryDialog::onItemClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &GalleryDialog::onDelete);

    refreshList();
}

void GalleryDialog::refreshList()
{
    m_list->clear();

    QDir dir(m_dir);
    QStringList filters;
    filters << "photo_*.jpg" << "video_*.avi";
    dir.setNameFilters(filters);
    dir.setSorting(QDir::Time | QDir::Reversed);

    QFileInfoList files = dir.entryInfoList(QDir::Files);
    if (files.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem("暂无照片或视频");
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setSizeHint(QSize(200, 100));
        m_list->addItem(emptyItem);
        return;
    }

    for (const QFileInfo &fi : files) {
        QString type = fi.suffix().toLower() == "jpg" ? "photo" : "video";
        QString label = QString("%1\n%2 KB")
                        .arg(fi.fileName())
                        .arg(fi.size() / 1024);

        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setData(Qt::UserRole + 1, type);

        if (type == "photo") {
            QPixmap thumb(fi.absoluteFilePath());
            if (!thumb.isNull()) {
                item->setIcon(QIcon(thumb.scaled(120, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            }
        }

        item->setSizeHint(QSize(140, 120));
        m_list->addItem(item);
    }
}

void GalleryDialog::onItemClicked(QListWidgetItem *item)
{
    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty())
        return;

    QString type = item->data(Qt::UserRole + 1).toString();
    if (type == "video")
        return;

    m_currentFile = filePath;
    QPixmap pix(filePath);
    if (pix.isNull()) {
        m_preview->setText("无法加载图片");
        return;
    }

    m_preview->setPixmap(pix.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_listPage->hide();
    m_previewPage->show();
}

void GalleryDialog::onDelete()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty())
        return;

    QFile::remove(filePath);
    refreshList();
}

void GalleryDialog::onBack()
{
    m_previewPage->hide();
    m_listPage->show();
}