#ifndef FILEMANAGERWIDGET_H
#define FILEMANAGERWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFileInfoList>
#include <QDir>

class FileManagerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileManagerWidget(QWidget *parent = nullptr);
    ~FileManagerWidget();

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onGoUp();
    void onRefresh();

private:
    void populateList();

    QListWidget *m_list;
    QLabel      *m_pathLabel;
    QPushButton *m_upBtn;
    QPushButton *m_refreshBtn;
    QPushButton *m_exitBtn;
    QDir         m_currentDir;
};

#endif