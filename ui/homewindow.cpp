#include "homewindow.h"
#include "ui_homewindow.h"
#include <QDateTime>
#include <QFileDialog>
#include <QUrl>
#include <QKeyEvent>
#include <thread>
#include <functional>
#include <iostream>
#include <iostream>
#include <chrono>
#include "ffmsg.h"
#include "globalhelper.h"
#include "toast.h"
#include "urldialog.h"
#include "easylogging++.h"


//获取当前毫秒时间戳
int64_t get_ms()
{
    std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
                                       std::chrono::system_clock::now().time_since_epoch()
                                   );
    return ms.count();
}

// 只有认定为直播流，才会触发变速机制
static int is_realtime(const char *url)
{
    if(   !strcmp(url, "rtp")
          || !strcmp(url, "rtsp")
          || !strcmp(url, "sdp")
          || !strcmp(url, "udp")
          || !strcmp(url, "rtmp")
      ) {
        return 1;
    }
    return 0;
}

HomeWindow::HomeWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HomeWindow)
{
    ui->setupUi(this);
    ui->playList->Init();
    QString str = QString("%1:%2:%3").arg(0, 2, 10, QLatin1Char('0')).arg(0, 2, 10, QLatin1Char('0')).arg(0, 2, 10, QLatin1Char('0'));
    ui->curPosition->setText(str);
    ui->totalDuration->setText(str);
    // 初始化为0ms
    ui->audioBufEdit->setText("0ms");
    ui->videoBufEdit->setText("0ms");
    ui->bufDurationBox->setCurrentIndex(3);
    ui->jitterBufBox->setCurrentIndex(1);
    max_cache_duration_ = 400;  // 默认200ms
    network_jitter_duration_ = 100; // 默认100ms
    initUi();
    InitSignalsAndSlots();
    // 初始布局
    resizeUI();
}

HomeWindow::~HomeWindow()
{
    delete ui;
}

void HomeWindow::initUi()
{
    //加载样式
    QString qss = GlobalHelper::GetQssStr("://res/qss/homewindow.css");
    setStyleSheet(qss);

    // 设置按钮图标
    int iconSize = 24;
    ui->playOrPauseBtn->setIcon(QIcon(":/res/play.png"));
    ui->playOrPauseBtn->setIconSize(QSize(iconSize, iconSize));
    ui->playOrPauseBtn->setText("");

    ui->stopBtn->setIcon(QIcon(":/res/stop.png"));
    ui->stopBtn->setIconSize(QSize(iconSize, iconSize));
    ui->stopBtn->setText("");

    ui->backFastBtn->setIcon(QIcon(":/res/backward.png"));
    ui->backFastBtn->setIconSize(QSize(iconSize, iconSize));
    ui->backFastBtn->setText("");

    ui->forwardFastBtn->setIcon(QIcon(":/res/forward.png"));
    ui->forwardFastBtn->setIconSize(QSize(iconSize, iconSize));
    ui->forwardFastBtn->setText("");

    ui->prevBtn->setIcon(QIcon(":/res/prev.png"));
    ui->prevBtn->setIconSize(QSize(iconSize, iconSize));
    ui->prevBtn->setText("");

    ui->nextBtn->setIcon(QIcon(":/res/next.png"));
    ui->nextBtn->setIconSize(QSize(iconSize, iconSize));
    ui->nextBtn->setText("");

    ui->FullScreenBtn->setIcon(QIcon(":/res/fullscreen.png"));
    ui->FullScreenBtn->setIconSize(QSize(iconSize, iconSize));
    ui->FullScreenBtn->setText("");

    ui->listBtn->setIcon(QIcon(":/res/playlist.png"));
    ui->listBtn->setIconSize(QSize(iconSize, iconSize));
    ui->listBtn->setText("");
}

int HomeWindow::InitSignalsAndSlots()
{
    connect(ui->playList, &Playlist::SigPlay, this, &HomeWindow::play); //播放列表双击播放 → play() 函数
    connect(this, &HomeWindow::sig_stopped, this, &HomeWindow::stop);
    // 设置play进度条的值
    ui->playSlider->setMinimum(0);
    play_slider_max_value = 6000;
    ui->playSlider->setMaximum(play_slider_max_value);
    //设置更新进度条的回调
    connect(this, &HomeWindow::sig_updateCurrentPosition, this, &HomeWindow::on_updateCurrentPosition);
    // 设置音量进度条的值
    ui->volumeSlider->setMinimum(0);
    ui->volumeSlider->setMaximum(100);
    ui->volumeSlider->setValue(20);
    //一个连接（Connection）是由 (发送者, 信号, 接收者, 槽) 这个四元组唯一确定的
    connect(ui->playSlider, &CustomSlider::SigCustomSliderValueChanged, this, &HomeWindow::on_playSliderValueChanged);
    connect(ui->volumeSlider, &CustomSlider::SigCustomSliderValueChanged, this, &HomeWindow::on_volumeSliderValueChanged);
    // toast提示不能直接在非ui线程显示，所以通过信号槽的方式触发提示
    // 自定义信号槽变量类型要注册，参考:https://blog.csdn.net/Larry_Yanan/article/details/127686354
    qRegisterMetaType<Toast::Level>("Toast::Level");
    connect(this, &HomeWindow::sig_showTips, this, &HomeWindow::on_showTips);
    qRegisterMetaType<int64_t>("int64_t");
    connect(this, &HomeWindow::sig_updateAudioCacheDuration, this, &HomeWindow::on_UpdateAudioCacheDuration);
    connect(this, &HomeWindow::sig_updateVideoCacheDuration, this, &HomeWindow::on_UpdateVideoCacheDuration);
    // 打开文件
    connect(ui->openFileAction, &QAction::triggered, this, &HomeWindow::on_openFile);
    connect(ui->openUrlAction, &QAction::triggered, this, &HomeWindow::on_openNetworkUrl);
    // 方向菜单 - 画面变换
    connect(ui->SetRotationNormal, &QAction::triggered, this, &HomeWindow::on_SetRotationNormal);
    connect(ui->SetRotation90CW, &QAction::triggered, this, &HomeWindow::on_SetRotation90CW);
    connect(ui->SetMirrorFlip, &QAction::triggered, this, &HomeWindow::on_SetMirrorFlip);
    connect(this, &HomeWindow::sig_updatePlayOrPause, this, &HomeWindow::on_updatePlayOrPause);
    return 0;
}

int HomeWindow::message_loop(void *arg)
{
    QString tips;
    IjkMediaPlayer *mp = (IjkMediaPlayer *)arg;
    // 线程循环
    LOG(INFO) << "message_loop into";
    while (1) {
        AVMessage msg;
        //取消息队列的消息，如果没有消息就阻塞，直到有消息被发到消息队列。
        // 这里先用非阻塞，在此线程可以做其他监测任务
        int retval = mp->ijkmp_get_msg(&msg, 0);    // 主要处理Java->C的消息
        if (retval < 0) {
            break;      // -1 要退出线程循环了
        }
        if(retval != 0)
            switch (msg.what) {
                case FFP_MSG_FLUSH:
                    LOG(INFO) <<  " FFP_MSG_FLUSH";
                    break;
                case FFP_MSG_PREPARED:
                    LOG(INFO) <<  " FFP_MSG_PREPARED" ;
                    mp->ijkmp_start();
                    //            ui->playOrPauseBtn->setText("暂停");
                    break;
                case FFP_MSG_FIND_STREAM_INFO:
                    LOG(INFO) <<  " FFP_MSG_FIND_STREAM_INFO";
                    getTotalDuration();
                    break;
                case FFP_MSG_PLAYBACK_STATE_CHANGED:
                    if(mp_->ijkmp_get_state() == MP_STATE_STARTED) {
                        emit sig_updatePlayOrPause(MP_STATE_STARTED);
                    }
                    if(mp_->ijkmp_get_state() == MP_STATE_PAUSED) {
                        emit sig_updatePlayOrPause(MP_STATE_PAUSED);
                    }
                    break;
                case FFP_MSG_SEEK_COMPLETE:
                    req_seeking_ = false;
                    //             startTimer();
                    break;
                case FFP_MSG_SCREENSHOT_COMPLETE:
                    if(msg.arg1 == 0 ) {
                        tips.sprintf("截屏成功,存储路径:%s", (char *)msg.obj);
                        emit sig_showTips(Toast::INFO, tips);
                    } else {
                        tips.sprintf("截屏失败, ret:%d", msg.arg1);
                        emit sig_showTips(Toast::WARN, tips);
                    }
                    req_screenshot_ = false;
                    break;
                case FFP_MSG_PLAY_FNISH:
                    tips.sprintf("播放完毕");
                    emit sig_showTips(Toast::INFO, tips);
                    // 发送播放完毕的信号触发调用停止函数
                    emit sig_stopped(); // 触发停止
                    break;
                default:
                    if(retval != 0) {
                        LOG(WARNING)  <<  " default " << msg.what ;
                    }
                    break;
            }
        msg_free_res(&msg);
        //        LOG(INFO) << "message_loop sleep, mp:" << mp;
        // 先模拟线程运行
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        // 获取缓存的值
        reqUpdateCacheDuration();
        // 获取当前播放位置
        reqUpdateCurrentPosition();
    }
//    LOG(INFO) << "message_loop leave";
    return 0;
}

int HomeWindow::OutputVideo(const Frame *frame)
{
    // 调用显示控件
    return ui->display->Draw(frame);
}

void HomeWindow::resizeEvent(QResizeEvent *event)
{
    resizeUI();
}

void HomeWindow::resizeUI()
{
    // 获取centralWidget的尺寸，所有子部件都在centralWidget内
    QWidget *central = ui->centralWidget;
    int cw_width = central->width();
    int cw_height = central->height();

    LOG(INFO) << "centralWidget width: " << cw_width << ", height: " << cw_height;

    // 1. 设置ctrlBar的位置：在centralWidget底部
    QRect rect = ui->ctrlBar->geometry();
    LOG(INFO) << "ctrlBar geometry: x=" << rect.x() << ", y=" << rect.y()
              << ", width=" << rect.width() << ", height=" << rect.height();

    // 确保ctrlBar高度有效
    int ctrlBarHeight = rect.height();
    if (ctrlBarHeight <= 0) {
        ctrlBarHeight = ui->ctrlBar->height();
        LOG(INFO) << "ctrlBar height from geometry <= 0, using height(): " << ctrlBarHeight;
    }
    if (ctrlBarHeight <= 0) {
        ctrlBarHeight = 95; // UI文件中的默认高度
        LOG(INFO) << "ctrlBar height still <= 0, using default: " << ctrlBarHeight;
    }

    // 计算ctrlBar的y位置：在centralWidget底部
    int ctrlBarY = cw_height - ctrlBarHeight;
    LOG(INFO) << "Setting ctrlBar: y=" << ctrlBarY << ", width=" << cw_width << ", height=" << ctrlBarHeight;

    // 设置ctrlBar的位置和大小
    rect.setRect(0, ctrlBarY, cw_width, ctrlBarHeight);
    ui->ctrlBar->setGeometry(rect);

    // 2. 设置display的位置：在centralWidget顶部，占据ctrlBar之上的空间
    int displayWidth = cw_width;
    int displayHeight = cw_height;

    // 减去ctrlBar的高度（如果ctrlBar没有被隐藏）
    if (!ui->ctrlBar->isHidden()) {
        displayHeight -= ctrlBarHeight;
    }

    // 是否显示文件列表
    if (is_show_file_list_) {
        displayWidth -= ui->playList->width();
    }

    LOG(INFO) << "Setting display: x=0, y=0, width=" << displayWidth << ", height=" << displayHeight;
    ui->display->setGeometry(0, 0, displayWidth, displayHeight);

    // 3. 设置文件列表的位置
    if (is_show_file_list_) {
        ui->playList->setGeometry(displayWidth, 0, ui->playList->width(), displayHeight);
    }

    // 4. 设置ctrlBar内部按钮的位置（这些是ctrlBar的子部件，坐标相对于ctrlBar）
    // 设置listBtn和settingBtn的位置
    rect = ui->settingBtn->geometry();
    int listBtnX = ui->ctrlBar->width() - rect.width() - rect.width() / 8 * 2;
    ui->listBtn->setGeometry(listBtnX, rect.y(), rect.width(), rect.height());

    rect = ui->listBtn->geometry();
    int settingBtnX = rect.x() - rect.width() - rect.width() / 8;
    ui->settingBtn->setGeometry(settingBtnX, rect.y(), rect.width(), rect.height());

    // 5. 设置播放进度条的长度，使其与display宽度一致
    rect = ui->playSlider->geometry();
    int playSliderWidth = displayWidth - 5 - 5;
    rect.setWidth(playSliderWidth);
    ui->playSlider->setGeometry(5, rect.y(), rect.width(), rect.height());

    // 6. 设置音量条位置，在ctrlBar的右侧
    rect = ui->volumeSlider->geometry();
    int volumeSliderX = ui->ctrlBar->width() - 5 - rect.width();
    ui->volumeSlider->setGeometry(volumeSliderX, rect.y(), rect.width(), rect.height());
}

void HomeWindow::on_UpdateAudioCacheDuration(int64_t duration)
{
    ui->audioBufEdit->setText(QString("%1ms").arg(duration));
}

void HomeWindow::on_UpdateVideoCacheDuration(int64_t duration)
{
    ui->videoBufEdit->setText(QString("%1ms").arg(duration));
}

// 弹出文件对话框，选择本地视频
void HomeWindow::on_openFile()
{
    QUrl url = QFileDialog::getOpenFileName(this, QStringLiteral("选择路径"), QDir::homePath(),
                                            nullptr,
                                            nullptr, QFileDialog::DontUseCustomDirectoryIcons);
    if(!url.toString().isEmpty()) {
        QFileInfo fileInfo(url.toString());
        // 停止状态下启动播放
        int ret = play(fileInfo.filePath().toStdString().c_str());
        // 并把该路径加入播放列表
        ui->playList->OnAddFile(url.toString());
    }
}

// 弹出对话框输入网络流地址
void HomeWindow::on_openNetworkUrl()
{
    UrlDialog urlDialog(this);
    int nResult = urlDialog.exec();
    if(nResult == QDialog::Accepted) {
        //
        QString url = urlDialog.GetUrl();
        //        LOG(INFO) << "Add url ok, url: " << url.toStdString();
        if(!url.isEmpty()) {
            //            LOG(INFO) << "SigAddFile url: " << url;
            //            emit SigAddFile(url);
            ui->playList->AddNetworkUrl(url);
            int ret = play(url.toStdString());
        }
    } else {
        LOG(INFO) << "Add url no";
    }
}

void HomeWindow::resizeCtrlBar()
{
}

void HomeWindow::resizeDisplayAndFileList()
{
}

int HomeWindow::seek(int cur_valule)
{
    if(mp_) {
        // 打印当前的值
        LOG(INFO) << "cur value " << cur_valule;
        double percent = cur_valule * 1.0 / play_slider_max_value;
        req_seeking_ = true;
        int64_t milliseconds = percent * total_duration_;
        mp_->ijkmp_seek_to(milliseconds);
        return 0;
    } else {
        return -1;
    }
}

int HomeWindow::fastForward(long inrc)
{
    if(mp_) {
        mp_->ijkmp_forward_to(inrc);
        return 0;
    } else {
        return -1;
    }
}

int HomeWindow::fastBack(long inrc)
{
    if(mp_) {
        mp_->ijkmp_back_to(inrc);
        return 0;
    } else {
        return -1;
    }
}

void HomeWindow::getTotalDuration()
{
    if(mp_) {
        total_duration_ = mp_->ijkmp_get_duration();
        // 更新信息
        // 当前播放位置，总时长
        long seconds = total_duration_ / 1000;
        int hour = int(seconds / 3600);
        int min = int((seconds - hour * 3600) / 60);
        int sec = seconds % 60;
        //QString格式化arg前面自动补0
        QString str = QString("%1:%2:%3").arg(hour, 2, 10, QLatin1Char('0')).arg(min, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
        ui->totalDuration->setText(str);
    }
}

void HomeWindow::reqUpdateCurrentPosition()
{
    int64_t cur_time = get_ms();
    if(cur_time - pre_get_cur_pos_time_ > 500) {
        pre_get_cur_pos_time_ = cur_time;
        // 播放器启动,并且不是请求seek的时候才去读取最新的播放位置
        //         LOG(INFO) << "reqUpdateCurrentPosition ";
        if(mp_ && !req_seeking_) {
            current_position_ = mp_->ijkmp_get_current_position();
            //            LOG(INFO) << "current_position_ " << current_position_;
            emit sig_updateCurrentPosition(current_position_);
        }
    }
}

void HomeWindow::reqUpdateCacheDuration()
{
    int64_t cur_time = get_ms();
    if(cur_time - pre_get_cache_time_ > 500) {
        pre_get_cache_time_ = cur_time;
        if(mp_) {
            audio_cache_duration =  mp_->ijkmp_get_property_int64(FFP_PROP_INT64_AUDIO_CACHED_DURATION, 0);
            video_cache_duration  =  mp_->ijkmp_get_property_int64(FFP_PROP_INT64_VIDEO_CACHED_DURATION, 0);
            emit sig_updateAudioCacheDuration(audio_cache_duration);
            emit sig_updateVideoCacheDuration(video_cache_duration);
            //是否触发变速播放取决于是不是实时流，这里由业务判断，目前主要是判断rtsp、rtmp、rtp流为直播流，有些比较难判断，比如httpflv既可以做直播也可以是点播
            if(real_time_ ) {
                if(audio_cache_duration > max_cache_duration_ + network_jitter_duration_) {
                    // 请求开启变速播放
                    mp_->ijkmp_set_playback_rate(accelerate_speed_factor_);
                    is_accelerate_speed_ = true;
                }
                if(is_accelerate_speed_ && audio_cache_duration < max_cache_duration_) {
                    mp_->ijkmp_set_playback_rate(normal_speed_factor_);
                    is_accelerate_speed_ = false;
                }
            }
        }
    }
}

void HomeWindow::on_listBtn_clicked()
{
    if(is_show_file_list_) {
        is_show_file_list_ = false;
        ui->playList->hide();
    } else {
        is_show_file_list_ = true;
        ui->playList->show();
    }
    resizeUI();
}

void HomeWindow::on_playOrPauseBtn_clicked()
{
    LOG(INFO) << "OnPlayOrPause call";
    bool ret = false;
    if(!mp_) {
        std::string url = ui->playList->GetCurrentUrl();
        if(!url.empty()) {
            // 停止状态下启动播放
            ret = play(url);
        }
    } else {
        if(mp_->ijkmp_get_state() == MP_STATE_STARTED) {
            // 设置为暂停
            mp_->ijkmp_pause();
            //            ui->playOrPauseBtn->setText("播放");
        } else if(mp_->ijkmp_get_state() == MP_STATE_PAUSED) {
            // 恢复播放
            mp_->ijkmp_start();
            //            ui->playOrPauseBtn->setText("暂停");
        }
    }
}

void HomeWindow::on_updatePlayOrPause(int state)
{
    if(state == MP_STATE_STARTED) {
        ui->playOrPauseBtn->setIcon(QIcon(":/res/pause.png"));
    } else  {
        ui->playOrPauseBtn->setIcon(QIcon(":/res/play.png"));
    }
    LOG(INFO) << "play state: " << state;
}

void HomeWindow::on_stopBtn_clicked()
{
    LOG(INFO) << "OnStop call";
    stop();
}

// stop -> play的转换
bool HomeWindow::play(std::string url)
{
    int ret = 0;
    // 如果本身处于播放状态则先停止原有的播放
    if(mp_) {
        stop();
    }
    // 1. 先检测mp是否已经创建
    real_time_ = is_realtime(url.c_str());
    is_accelerate_speed_ = false;
    mp_ = new IjkMediaPlayer();
    //1.1 创建
    ret = mp_->ijkmp_create(std::bind(&HomeWindow::message_loop, this, std::placeholders::_1));
    if(ret < 0) {
        LOG(ERROR) << "IjkMediaPlayer create failed";
        delete mp_;
        mp_ = NULL;
        return false;
    }
    mp_->AddVideoRefreshCallback(std::bind(&HomeWindow::OutputVideo, this,
                                           std::placeholders::_1));
    // 1.2 设置url
    mp_->ijkmp_set_data_source(url.c_str());
    mp_->ijkmp_set_playback_volume(ui->volumeSlider->value());
    // 1.3 准备工作
    ret = mp_->ijkmp_prepare_async();
    if(ret < 0) {
        LOG(ERROR) << "IjkMediaPlayer create failed";
        delete mp_;
        mp_ = NULL;
        return false;
    }
    ui->display->StartPlay();
    startTimer();
    return true;
}

void HomeWindow::on_updateCurrentPosition(long position)
{
    // 更新信息
    // 当前播放位置，总时长
    long seconds = position / 1000;
    int hour = int(seconds / 3600);
    int min = int((seconds - hour * 3600) / 60);
    int sec = seconds % 60;
    //QString格式化arg前面自动补0
    QString str = QString("%1:%2:%3").arg(hour, 2, 10, QLatin1Char('0')).arg(min, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
    if((position <=  total_duration_)   // 如果不是直播，那播放时间该<= 总时长
       || (total_duration_ == 0)) { // 如果是直播，此时total_duration_为0
        ui->curPosition->setText(str);
    }
    // 更新进度条
    if(total_duration_ > 0) {
        int pos = current_position_ * 1.0 / total_duration_ * ui->playSlider->maximum();
        ui->playSlider->setValue(pos);
    }
}

void HomeWindow::onTimeOut()
{
    if(mp_) {
        //        reqUpdateCurrentPosition();
    }
}

void HomeWindow::on_playSliderValueChanged(int value)
{
    seek(value);
}

void HomeWindow::on_volumeSliderValueChanged(int value)
{
    if(mp_) {
        mp_->ijkmp_set_playback_volume(value);
    }
}

bool HomeWindow::stop()
{
    if(mp_) {
        stopTimer();
        mp_->ijkmp_stop();
        mp_->ijkmp_destroy();
        delete mp_;
        mp_ = NULL;
        real_time_ = 0;
        is_accelerate_speed_ = false;
        ui->display->StopPlay();        // 停止渲染，后续刷黑屏
        ui->playOrPauseBtn->setIcon(QIcon(":/res/play.png"));
        return 0;
    } else {
        return -1;
    }
}

void HomeWindow::on_speedBtn_clicked()
{
    if(mp_) {
        // 先获取当前的倍速,每次叠加0.5, 支持0.5~2.0倍速
        float rate =  mp_->ijkmp_get_playback_rate() + 0.25;
        if(rate > 2.0) {
            rate = 0.25;
        }
        mp_->ijkmp_set_playback_rate(rate);
        ui->speedBtn->setText(QString("倍速:%1").arg(rate));
    }
}

bool HomeWindow::resume()
{
    if(mp_) {
        mp_->ijkmp_start();
        return 0;
    } else {
        return -1;
    }
}

bool HomeWindow::pause()
{
    if(mp_) {
        mp_->ijkmp_pause();
        return 0;
    } else {
        return -1;
    }
}

void HomeWindow::on_screenBtn_clicked()
{
    if(mp_) {
        QDateTime time = QDateTime::currentDateTime();
        // 比如 20230513-161813-769.jpg
        QString dateTime = time.toString("yyyyMMdd-hhmmss-zzz") + ".jpg";
        mp_->ijkmp_screenshot((char *)dateTime.toStdString().c_str());
    }
}

void HomeWindow::on_showTips(Toast::Level leve, QString tips)
{
    Toast::instance().show(leve, tips);
}

void HomeWindow::on_prevBtn_clicked()
{
    // 停止当前的播放，然后播放下一个，这里就需要播放列表配合
    //获取前一个播放的url 并将对应的url选中
    std::string url = ui->playList->GetPrevUrlAndSelect();
    if(!url.empty()) {
        play(url);
    } else {
        emit sig_showTips(Toast::ERROR, "没有可以播放的URL");
    }
}

void HomeWindow::on_nextBtn_clicked()
{
    std::string url = ui->playList->GetNextUrlAndSelect();
    if(!url.empty()) {
        play(url);
    } else {
        emit sig_showTips(Toast::ERROR, "没有可以播放的URL");
    }
}

void HomeWindow::on_forwardFastBtn_clicked()
{
    fastForward(MP_SEEK_STEP);
}

void HomeWindow::on_backFastBtn_clicked()
{
    fastBack(-1 * MP_SEEK_STEP);
}

void HomeWindow::on_bufDurationBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        max_cache_duration_ = 30;
        break;
    case 1:
        max_cache_duration_ = 100;
        break;
    case 2:
        max_cache_duration_ = 200;
        break;
    case 3:
        max_cache_duration_ = 400;
        break;
    case 4:
        max_cache_duration_ = 600;
        break;
    case 5:
        max_cache_duration_ = 800;
        break;
    case 6:
        max_cache_duration_ = 1000;
        break;
    case 7:
        max_cache_duration_ = 2000;
        break;
    case 8:
        max_cache_duration_ = 4000;
        break;
    default:
        break;
    }
}

void HomeWindow::on_jitterBufBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        max_cache_duration_ = 30;
        break;
    case 1:
        max_cache_duration_ = 100;
        break;
    case 2:
        max_cache_duration_ = 200;
        break;
    case 3:
        max_cache_duration_ = 400;
        break;
    case 4:
        max_cache_duration_ = 600;
        break;
    case 5:
        max_cache_duration_ = 800;
        break;
    case 6:
        max_cache_duration_ = 1000;
        break;
    case 7:
        max_cache_duration_ = 2000;
        break;
    case 8:
        max_cache_duration_ = 4000;
        break;
    default:
        break;
    }
}

void HomeWindow::startTimer()
{
    if(play_time_) {
        play_time_->stop();
        delete play_time_;
        play_time_ = nullptr;
    }
    play_time_ = new QTimer();
    play_time_->setInterval(1000);  // 1秒触发一次
    connect(play_time_, SIGNAL(timeout()), this, SLOT(onTimeOut()));
    play_time_->start();
}

void HomeWindow::stopTimer()
{
    if(play_time_) {
        play_time_->stop();
        delete play_time_;
        play_time_ = nullptr;
    }
}


void HomeWindow::on_SetRotationNormal() {
    current_rotation_ = 0;
    mirror_h_state_ = false;
    mirror_v_state_ = false;
    ui->display->ResetTransform();
}

void HomeWindow::on_SetRotation90CW() {
    // 循环切换：0 -> 90 -> 180 -> 270 -> 0
    current_rotation_ = (current_rotation_ + 90) % 360;
    ui->display->SetRotation(current_rotation_);
}

void HomeWindow::on_SetMirrorFlip() {
    // 水平镜像切换（toggle）
    mirror_h_state_ = !mirror_h_state_;
    ui->display->SetMirror(mirror_h_state_, mirror_v_state_);
}



//全屏
void HomeWindow::on_FullScreenBtn_clicked()
{
    if (isFullScreen()) {
        // 退出全屏
        showNormal();
        ui->menuBar->show();
        ui->ctrlBar->show();
        if (is_show_file_list_) {
            ui->playList->show();
        }
        // 恢复窗口样式
        setStyleSheet(GlobalHelper::GetQssStr("://res/qss/homewindow.css"));
    } else {
        // 进入全屏
        showFullScreen();
        ui->menuBar->hide();
        ui->ctrlBar->hide();
        ui->playList->hide();
        // 全屏时保留暗色背景样式，避免黑边处显示白色
        setStyleSheet(R"(
            QMainWindow { background-color: #1a1a1a; }
            QWidget#centralWidget { background-color: #1a1a1a; }
            QWidget#display { background-color: #000000; }
        )");
    }
    resizeUI();
}

void HomeWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        // ESC键退出全屏
        showNormal();
        ui->menuBar->show();
        ui->ctrlBar->show();
        if (is_show_file_list_) {
            ui->playList->show();
        }
        setStyleSheet(GlobalHelper::GetQssStr("://res/qss/homewindow.css"));
        resizeUI();
        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}