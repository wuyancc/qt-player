#ifndef MEDIALIST_H
#define MEDIALIST_H


#include <QListWidget>
#include <QMenu>
#include <QAction>
class MediaList : public QListWidget
{
    Q_OBJECT

public:
    MediaList(QWidget *parent = 0);
    ~MediaList();
    bool Init();
protected:
    void contextMenuEvent(QContextMenuEvent* event);
    void dropEvent(QDropEvent *event);  // 重写drop事件以捕获拖拽完成
public:
    void AddFile(); //添加文件
    void AddUrl();  // 添加网络地址
    void RemoveFile();
signals:
    void SigAddFile(QString strFileName);   //添加文件信号
    void SigItemMoved(int fromRow, int toRow);  // 文件位置移动信号


private:
    QMenu menu_;

    QAction m_stActAddFile;     //添加文件
    QAction m_stActAddUrl;      // 添加网络URL
    QAction m_stActRemove;      //移除文件
    QAction m_stActClearList;   //清空列表
};

#endif // MEDIALIST_H
