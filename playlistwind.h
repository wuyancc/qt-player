#ifndef PLAYLISTWIND_H
#define PLAYLISTWIND_H

#include <QWidget>
#include <QListWidgetItem>
#include <QDragEnterEvent>
#include <QMimeData>
namespace Ui {
class PlayListWind;
}

class PlayListWind : public QWidget
{
    Q_OBJECT

public:
    explicit PlayListWind(QWidget *parent = 0);
    ~PlayListWind();

    bool Init();


    /**
     * @brief	获取播放列表状态
     *
     * @return	true 显示 false 隐藏
     * @note
     */
    bool GetPlaylistStatus();
    int GetCurrentIndex();
public:
    /**
     * @brief	添加文件
     *
     * @param	strFileName 文件完整路径
     * @note
     */
    void OnAddFile(QString strFileName);
    void OnAddFileAndPlay(QString strFileName);

    void OnBackwardPlay();
    void OnForwardPlay();
    void OnRequestPlayCurrentFile();
    /* 在这里定义dock的初始大小 */
    QSize sizeHint() const
    {
        return QSize(150, 900);
    }

    /**
    * @brief	放下事件
    *
    * @param	event 事件指针
    * @note
    */
    void dropEvent(QDropEvent *event);
    /**
    * @brief	拖动事件
    *
    * @param	event 事件指针
    * @note
    */
    void dragEnterEvent(QDragEnterEvent *event);

signals:
    void SigUpdateUi();	//< 界面排布更新
    void SigPlay(QString strFile); //< 播放文件

private:
    bool InitUi();
    bool ConnectSignalSlots();

private slots:

    void on_List_itemDoubleClicked(QListWidgetItem *item);

    void on_List_itemSelectionChanged();
private:
    Ui::PlayListWind *ui;
    int m_nCurrentPlayListIndex = 0;
};

#endif // PLAYLISTWIND_H
