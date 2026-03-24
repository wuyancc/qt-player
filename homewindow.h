#ifndef HOMEWINDOW_H
#define HOMEWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "toast.h"
#include "ijkmediaplayer.h"
namespace Ui
{
class HomeWindow;
}

class HomeWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HomeWindow(QWidget *parent = 0);
    ~HomeWindow();
    void initUi();
    int InitSignalsAndSlots();
    int message_loop(void *arg);
    int OutputVideo(const Frame *frame);

protected:
    virtual void  resizeEvent(QResizeEvent *event);
    virtual void  keyPressEvent(QKeyEvent *event);
    void resizeUI();
signals:
    // 发送要显示的提示信息
    void sig_showTips(Toast::Level leve, QString tips);
    void sig_updateAudioCacheDuration(int64_t duration);
    void sig_updateVideoCacheDuration(int64_t duration);
    void sig_updateCurrentPosition(long position);
    void sig_updatePlayOrPause(int state);

    void sig_stopped(); // 被动停止
private slots:
    void on_UpdateAudioCacheDuration(int64_t duration);
    void on_UpdateVideoCacheDuration(int64_t duration);
    // 打开文件
    void on_openFile();
    // 打开网络流，逻辑和vlc类似
    void on_openNetworkUrl();
    void on_listBtn_clicked();
    void on_playOrPauseBtn_clicked();       // 播放暂停
    void on_updatePlayOrPause(int state);
    void on_stopBtn_clicked();  // 停止
    // stop->play
    bool play(std::string url);
    bool stop();

    // 进度条响应
    // 拖动触发
    //    void on_playSliderValueChanged();
    //    void on_volumeSliderValueChanged();
    void on_updateCurrentPosition(long position);

    void onTimeOut();

    void on_playSliderValueChanged(int value); //调用该函数后--》调用seek();
    void on_volumeSliderValueChanged(int value);
    void on_speedBtn_clicked();

    void on_screenBtn_clicked();
    void on_showTips(Toast::Level leve, QString tips);
    void on_bufDurationBox_currentIndexChanged(int index);

    void on_jitterBufBox_currentIndexChanged(int index);

    void on_prevBtn_clicked();

    void on_nextBtn_clicked();

    void on_forwardFastBtn_clicked();

    void on_backFastBtn_clicked();

    //画面翻转相关
    void on_SetRotationNormal();
    void on_SetRotation90CW();
    void on_SetMirrorFlip();

    void on_FullScreenBtn_clicked();

private:

    void startTimer();
    void stopTimer();
    // pause->play
    bool resume();
    // play->pause
    bool pause();
    // play/pause->stop

    void resizeCtrlBar();
    void resizeDisplayAndFileList();
    int seek(int cur_valule);

    int fastForward(long inrc);
    int fastBack(long inrc);
    // 主动获取信息，并更新到ui
    void getTotalDuration();

    // 定时器获取，每秒读取一次时间
    void reqUpdateCurrentPosition();
    void reqUpdateCacheDuration();

    // 画面变换状态
    int current_rotation_ = 0;      // 当前旋转角度
    bool mirror_h_state_ = false;   // 水平镜像状态
    bool mirror_v_state_ = false;   // 垂直镜像状态

private:
    Ui::HomeWindow *ui;

    bool is_show_file_list_ = true;     // 是否显示文件列表，默认显示
    IjkMediaPlayer *mp_ = NULL;

    //播放相关的信息
    // 当前文件播放的总长度,单位为ms
    long total_duration_ = 0;
    long current_position_ = 0;
    int64_t pre_get_cur_pos_time_ = 0;

    QTimer *play_time_ = nullptr;

    int play_slider_max_value = 6000;
    bool req_seeking_ = false;     //当请求seek时，中间产生的播放速度不

    bool req_screenshot_ = false;


    // 缓存统计
    int max_cache_duration_ = 400;  // 默认200ms
    int network_jitter_duration_ = 100; // 默认100ms
    float accelerate_speed_factor_ = 1.2; //默认加速是1.2
    float normal_speed_factor_ = 1.0;     // 正常播放速度1.0
    bool  is_accelerate_speed_ = false;

    // 缓存长度
    int64_t audio_cache_duration = 0;
    int64_t video_cache_duration = 0;
    int64_t pre_get_cache_time_ = 0;
    int real_time_ = 0;

    // 码率
    int64_t audio_bitrate_duration = 0;
    int64_t video_bitrate_duration = 0;
};

#endif // HOMEWINDOW_H
