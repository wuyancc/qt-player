#ifndef TOAST_H
#define TOAST_H
/**
 * @brief The Toast class
 *     源码参考:Qt实现Toast提示消息 https://blog.csdn.net/wwwlyj123321/article/details/112391884
 */


#include <QObject>
#include <QRect>
class ToastDlg;

class Toast: QObject
{
public:
    enum Level
    {
      INFO, WARN, ERROR
    };
private:
    Toast();
public:
    static Toast& instance();
public:
    void show(Level level, const QString& text);

private:
    void timerEvent(QTimerEvent *event) override;

private:
    ToastDlg* dlg_;
    int timer_id_{0};
    QRect geometry_;
};
#endif // TOAST_H
