#include <QContextMenuEvent>
#include <QFileDialog>
#include "medialist.h"
#include "urldialog.h"
#include "easylogging++.h"
//#include <QDebug>
//#define LOG(INFO) qDebug()
#pragma execution_character_set("utf-8")

MediaList::MediaList(QWidget *parent)
    : QListWidget(parent),
      menu_(this),
      m_stActAddFile(this),
      m_stActAddUrl(this),
      m_stActRemove(this),
      m_stActClearList(this)
{
}

MediaList::~MediaList()
{
}

bool MediaList::Init()
{
    // 启用内部拖拽排序
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);

    m_stActAddFile.setText("添加文件");
    menu_.addAction(&m_stActAddFile);
    m_stActAddUrl.setText("添加地址");
    menu_.addAction(&m_stActAddUrl);
    m_stActRemove.setText("移除");
    menu_.addAction(&m_stActRemove);
    m_stActClearList.setText("清空列表");
    menu_.addAction(&m_stActClearList);
    connect(&m_stActAddFile, &QAction::triggered, this, &MediaList::AddFile);
    connect(&m_stActAddUrl, &QAction::triggered, this, &MediaList::AddUrl);
    connect(&m_stActRemove, &QAction::triggered, this, &MediaList::RemoveFile);
    connect(&m_stActClearList, &QAction::triggered, this, &QListWidget::clear);
    return true;
}

void MediaList::contextMenuEvent(QContextMenuEvent* event)
{
    menu_.exec(event->globalPos());
}

void MediaList::AddFile()
{
    QStringList listFileName = QFileDialog::getOpenFileNames(this, "打开文件", QDir::homePath(),
                               "视频文件(*.ts *.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp *.wav *.mp3 *.aac)");
    for (QString strFileName : listFileName) {
        emit SigAddFile(strFileName);
    }
}

void MediaList::AddUrl()
{
    UrlDialog urlDialog(this);
    int nResult = urlDialog.exec();
    if(nResult == QDialog::Accepted) {
        //
        QString url = urlDialog.GetUrl();
        LOG(INFO) << "Add url ok, url: " << url.toStdString();
        if(!url.isEmpty()) {
            LOG(INFO) << "SigAddFile url: " << url.toStdString();
            emit SigAddFile(url);
        } else {
            LOG(ERROR) << "Add url no";
        }
    } else {
        LOG(WARNING) << "Add url Rejected";
    }
}

void MediaList::RemoveFile()
{
    takeItem(currentRow());
}

void MediaList::dropEvent(QDropEvent *event)
{
    int fromRow = currentRow();
    QListWidget::dropEvent(event);  // 调用父类实现完成拖拽
    int toRow = currentRow();

    if (fromRow != toRow) {
        emit SigItemMoved(fromRow, toRow);  // 通知位置发生变化
    }
}
