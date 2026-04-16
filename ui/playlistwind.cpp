#include <QFileInfo>
#include <QUrl>
#include "playlistwind.h"
#include "ui_playlistwind.h"
#include "globalhelper.h"
PlayListWind::PlayListWind(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PlayListWind)
{
    ui->setupUi(this);
//    this->setStyleSheet("QWidget { border: none; }");
    Init();
}


PlayListWind::~PlayListWind()
{
    QStringList strListPlayList;
    for (int i = 0; i < ui->list->count(); i++)
    {
        strListPlayList.append(ui->list->item(i)->toolTip());
    }
    GlobalHelper::SavePlaylist(strListPlayList);

    delete ui;
}

bool PlayListWind::Init()
{
    if (ui->list->Init() == false)
    {
        return false;
    }

    if (InitUi() == false)
    {
        return false;
    }

    if (ConnectSignalSlots() == false)
    {
        return false;
    }

    setAcceptDrops(true);

    return true;
}

bool PlayListWind::InitUi()
{
    setStyleSheet(GlobalHelper::GetQssStr(":/qss/res/qss/playlist.css"));
    ui->list->clear();

    QStringList strListPlaylist;
    GlobalHelper::GetPlaylist(strListPlaylist);

    for (QString strVideoFile : strListPlaylist)
    {
        QFileInfo fileInfo(strVideoFile);
        if (fileInfo.exists())
        {
            QListWidgetItem *pItem = new QListWidgetItem(ui->list);
            pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath()));  // 用户数据
            pItem->setText(QString("%1").arg(fileInfo.fileName()));  // 显示文本
            pItem->setToolTip(fileInfo.filePath());
            ui->list->addItem(pItem);
        }
    }
    if (strListPlaylist.length() > 0)
    {
        ui->list->setCurrentRow(0);
    }

    //ui->list->addItems(strListPlaylist);


    return true;
}

bool PlayListWind::ConnectSignalSlots()
{
    QList<bool> listRet;
    bool bRet;

    bRet = connect(ui->list, &MediaList::SigAddFile, this, &PlayListWind::OnAddFile);
    listRet.append(bRet);

    for (bool bReturn : listRet)
    {
        if (bReturn == false)
        {
            return false;
        }
    }

    return true;
}

void PlayListWind::on_List_itemDoubleClicked(QListWidgetItem *item)
{
    emit SigPlay(item->data(Qt::UserRole).toString());
    m_nCurrentPlayListIndex = ui->list->row(item);
    ui->list->setCurrentRow(m_nCurrentPlayListIndex);
}

bool PlayListWind::GetPlaylistStatus()
{
    if (this->isHidden())
    {
        return false;
    }

    return true;
}

int PlayListWind::GetCurrentIndex()
{
    return m_nCurrentPlayListIndex;
}

void PlayListWind::OnRequestPlayCurrentFile()
{
    if(ui->list->count() > 0)       // 有文件才会触发请求播放
    {
        on_List_itemDoubleClicked(ui->list->item(m_nCurrentPlayListIndex));
        ui->list->setCurrentRow(m_nCurrentPlayListIndex);
    }
}

void PlayListWind::OnAddFile(QString strFileName)
{
    bool bSupportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
        strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
        strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".3gp", Qt::CaseInsensitive) ||
        strFileName.startsWith("rtmp://", Qt::CaseInsensitive) ||
        strFileName.startsWith("https://", Qt::CaseInsensitive) ||
        strFileName.startsWith("http://", Qt::CaseInsensitive);
    if (!bSupportMovie)
    {
        return;
    }


    QFileInfo fileInfo(strFileName);
    QList<QListWidgetItem *> listItem = ui->list->findItems(fileInfo.fileName(), Qt::MatchExactly);
    QListWidgetItem *pItem = nullptr;
    if (listItem.isEmpty())
    {
        pItem = new QListWidgetItem(ui->list);
        pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath()));  // 用户数据
        pItem->setText(fileInfo.fileName());  // 显示文件名
        pItem->setToolTip(fileInfo.filePath()); // 完整文件路径
        ui->list->addItem(pItem);
    }
    else
    {
        pItem = listItem.at(0);
    }
}

void PlayListWind::OnAddFileAndPlay(QString strFileName)
{
    bool bSupportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
        strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
        strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
        strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
        strFileName.endsWith(".3gp", Qt::CaseInsensitive);
    if (!bSupportMovie)
    {
        return;
    }

    QFileInfo fileInfo(strFileName);
    QList<QListWidgetItem *> listItem = ui->list->findItems(fileInfo.fileName(), Qt::MatchExactly);
    QListWidgetItem *pItem = nullptr;
    if (listItem.isEmpty())
    {
        pItem = new QListWidgetItem(ui->list);
        pItem->setData(Qt::UserRole, QVariant(fileInfo.filePath()));  // 用户数据
        pItem->setText(fileInfo.fileName());  // 显示文本
        pItem->setToolTip(fileInfo.filePath());
        ui->list->addItem(pItem);
    }
    else
    {
        pItem = listItem.at(0);
    }
    on_List_itemDoubleClicked(pItem);
}

void PlayListWind::OnBackwardPlay()
{
    if (m_nCurrentPlayListIndex == 0)
    {
        m_nCurrentPlayListIndex = ui->list->count() - 1;
        on_List_itemDoubleClicked(ui->list->item(m_nCurrentPlayListIndex));
        ui->list->setCurrentRow(m_nCurrentPlayListIndex);
    }
    else
    {
        m_nCurrentPlayListIndex--;
        on_List_itemDoubleClicked(ui->list->item(m_nCurrentPlayListIndex));
        ui->list->setCurrentRow(m_nCurrentPlayListIndex);
    }
}

void PlayListWind::OnForwardPlay()
{
    if (m_nCurrentPlayListIndex == ui->list->count() - 1)
    {
        m_nCurrentPlayListIndex = 0;
        on_List_itemDoubleClicked(ui->list->item(m_nCurrentPlayListIndex));
        ui->list->setCurrentRow(m_nCurrentPlayListIndex);
    }
    else
    {
        m_nCurrentPlayListIndex++;
        on_List_itemDoubleClicked(ui->list->item(m_nCurrentPlayListIndex));
        ui->list->setCurrentRow(m_nCurrentPlayListIndex);
    }
}

void PlayListWind::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
    {
        return;
    }

    for (QUrl url : urls)
    {
        QString strFileName = url.toLocalFile();

        OnAddFile(strFileName);
    }
}

void PlayListWind::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void PlayListWind::on_List_itemSelectionChanged()
{
     m_nCurrentPlayListIndex = ui->list->currentIndex().row();      // 获取选中后的新位置。
}
