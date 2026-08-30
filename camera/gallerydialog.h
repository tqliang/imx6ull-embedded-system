#ifndef GALLERYDIALOG_H
#define GALLERYDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class GalleryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GalleryDialog(const QString &dir, QWidget *parent = nullptr);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onDelete();
    void onBack();

private:
    void refreshList();

    QString       m_dir;
    QListWidget  *m_list;
    QPushButton  *m_deleteBtn;
    QPushButton  *m_backBtn;
    QLabel       *m_preview;
    QWidget      *m_listPage;
    QWidget      *m_previewPage;
    QVBoxLayout  *m_stack;
    QString       m_currentFile;
};

#endif