#include "ijkmediaplayer.h"
#include <iostream>
#include <string.h>
#include "ffmsg.h"
#include "easylogging++.h"
IjkMediaPlayer::IjkMediaPlayer()
{
    LOG(INFO) << " IjkMediaPlayer()\n ";
}

IjkMediaPlayer::~IjkMediaPlayer()
{
    LOG(INFO) << " ~IjkMediaPlayer()\n ";
    // 此时需要停止线程
}

int IjkMediaPlayer::ijkmp_create(std::function<int (void *)> msg_loop)
{
    int ret = 0;
    ffplayer_ = new FFPlayer();
    if(!ffplayer_) {
        LOG(INFO) << " new FFPlayer() failed\n ";
        return -1;
    }
    msg_loop_ = msg_loop;
    ret = ffplayer_->ffp_create();
    if(ret < 0) {
        return -1;
    }
    return 0;
}

int IjkMediaPlayer::ijkmp_destroy()
{
    if(msg_thread_->joinable()) {
        LOG(INFO) <<  "call msg_queue_abort" ;
        msg_queue_abort(&ffplayer_->msg_queue_);
        LOG(INFO) <<  "wait msg loop abort" ;
        msg_thread_->join(); // 等待线程退出
    }

    ffplayer_->ffp_destroy();
    return 0;
}

// 这个方法的设计来源于Android mediaplayer, 其本意是
//int IjkMediaPlayer::ijkmp_set_data_source(Uri uri)
int IjkMediaPlayer::ijkmp_set_data_source(const char *url)
{
    if(!url) {
        return -1;
    }
    data_source_ = strdup(url); // 分配内存+ 拷贝字符串
    return 0;
}

int IjkMediaPlayer::ijkmp_prepare_async()
{
    // 判断mp的状态
    // 正在准备中
    mp_state_ = MP_STATE_ASYNC_PREPARING;
    // 启用消息队列
    msg_queue_start(&ffplayer_->msg_queue_);
    // 创建循环线程
    msg_thread_ = new std::thread(&IjkMediaPlayer::ijkmp_msg_loop, this, this);
    // 调用ffplayer
    int ret = ffplayer_->ffp_prepare_async_l(data_source_);
    if(ret < 0) {
        mp_state_ = MP_STATE_ERROR;
        return -1;
    }
    return 0;
}

int IjkMediaPlayer::ijkmp_start()
{
    ffp_notify_msg1(ffplayer_, FFP_REQ_START);
    return 0;
}

int IjkMediaPlayer::ijkmp_stop()
{
    int retval = ffplayer_->ffp_stop_l();
    if (retval < 0) {
        return retval;
    }
    return 0;
}

int IjkMediaPlayer::ijkmp_pause()
{
    // 发送暂停的操作命令
    ffp_remove_msg(ffplayer_, FFP_REQ_START);
    ffp_remove_msg(ffplayer_, FFP_REQ_PAUSE);
    ffp_notify_msg1(ffplayer_, FFP_REQ_PAUSE);
    return 0;
}

int IjkMediaPlayer::ijkmp_seek_to(long msec)
{
    seek_req = 1;
    seek_msec = msec;
    ffp_remove_msg(ffplayer_, FFP_REQ_SEEK);
    ffp_notify_msg2(ffplayer_, FFP_REQ_SEEK, (int)msec);
    return 0;
}

int IjkMediaPlayer::ijkmp_forward_to(long incr)
{
    seek_req = 1;
    ffp_remove_msg(ffplayer_, FFP_REQ_FORWARD);
    ffp_notify_msg2(ffplayer_, FFP_REQ_FORWARD, (int)incr);
    return 0;
}

int IjkMediaPlayer::ijkmp_back_to(long incr)
{
    seek_req = 1;
    ffp_remove_msg(ffplayer_, FFP_REQ_BACK);
    ffp_notify_msg2(ffplayer_, FFP_REQ_BACK, (int)incr);
    return 0;
}


// 请求截屏
int IjkMediaPlayer::ijkmp_screenshot(char *file_path)
{
    ffp_remove_msg(ffplayer_, FFP_REQ_SCREENSHOT);
    ffp_notify_msg4(ffplayer_, FFP_REQ_SCREENSHOT, 0, 0, file_path, strlen(file_path) + 1);
    return 0;
}

int IjkMediaPlayer::ijkmp_get_state()
{
    return mp_state_;
}

long IjkMediaPlayer::ijkmp_get_current_position()
{
    return ffplayer_->ffp_get_current_position_l();
}

long IjkMediaPlayer::ijkmp_get_duration()
{
    return ffplayer_->ffp_get_duration_l();
}

int IjkMediaPlayer::ijkmp_get_msg(AVMessage *msg, int block)
{
    int pause_ret = 0;
    while (1) {
        int continue_wait_next_msg = 0;
        //取消息，没有消息则根据block值 =1阻塞，=0不阻塞。
        int retval = msg_queue_get(&ffplayer_->msg_queue_, msg, block);
        if (retval <= 0) {      // -1 abort, 0 没有消息
            return retval;
        }
        switch (msg->what) {
            case FFP_MSG_PREPARED:
                LOG(INFO) <<  " FFP_MSG_PREPARED" ;
                //            ijkmp_change_state_l(MP_STATE_PREPARED);
                break;
            case FFP_REQ_START:
                LOG(INFO) <<  " FFP_REQ_START" ;
                continue_wait_next_msg = 1;
                retval = ffplayer_->ffp_start_l();
                if (retval == 0) {
                    ijkmp_change_state_l(MP_STATE_STARTED);
                }
                break;
            case FFP_REQ_PAUSE:
                continue_wait_next_msg = 1;
                pause_ret = ffplayer_->ffp_pause_l();
                if(pause_ret == 0) {
                    //设置为暂停暂停
                    ijkmp_change_state_l(MP_STATE_PAUSED);  // 暂停后怎么恢复？
                }
                break;
            case FFP_MSG_SEEK_COMPLETE:
                LOG(INFO) << "ijkmp_get_msg: FFP_MSG_SEEK_COMPLETE\n";
                seek_req = 0;
                seek_msec = 0;
                break;
            case FFP_REQ_SEEK:
                LOG(INFO) << "ijkmp_get_msg: FFP_REQ_SEEK\n";
                continue_wait_next_msg = 1;
                ffplayer_->ffp_seek_to_l(msg->arg1);
                break;
            case FFP_REQ_FORWARD:
                LOG(INFO) << "ijkmp_get_msg: FFP_REQ_FORWARD\n";
                continue_wait_next_msg = 1;
                ffplayer_->ffp_forward_to_l(msg->arg1);
                break;
            case FFP_REQ_BACK:
                LOG(INFO) << "ijkmp_get_msg: FFP_REQ_BACK\n";
                continue_wait_next_msg = 1;
                ffplayer_->ffp_back_to_l(msg->arg1);
                break;
            case FFP_REQ_SCREENSHOT:
                LOG(INFO) << "ijkmp_get_msg: FFP_REQ_SCREENSHOT: " << (char *)msg->obj ;
                continue_wait_next_msg = 1;
                ffplayer_->ffp_screenshot_l((char *)msg->obj);
                break;
            default:
                LOG(INFO) <<  " default " << msg->what ;
                break;
        }
        if (continue_wait_next_msg) {
            msg_free_res(msg);
            continue;
        }
        return retval;
    }
    return -1;
}

void IjkMediaPlayer::ijkmp_set_playback_volume(int volume)
{
    ffplayer_->ffp_set_playback_volume(volume);
}

int IjkMediaPlayer::ijkmp_msg_loop(void *arg)
{
    msg_loop_(arg);
    return 0;
}

void IjkMediaPlayer::ijkmp_set_playback_rate(float rate)
{
    ffplayer_->ffp_set_playback_rate(rate);
}

float IjkMediaPlayer::ijkmp_get_playback_rate()
{
    return ffplayer_->ffp_get_playback_rate();
}

void IjkMediaPlayer::AddVideoRefreshCallback(
    std::function<int (const Frame *)> callback)
{
    ffplayer_->AddVideoRefreshCallback(callback);
}

int64_t IjkMediaPlayer::ijkmp_get_property_int64(int id, int64_t default_value)
{
    return ffplayer_->ffp_get_property_int64(id, default_value);
    // return 0;
}

void IjkMediaPlayer::ijkmp_change_state_l(int new_state)
{
    mp_state_ = new_state;
    ffp_notify_msg1(ffplayer_, FFP_MSG_PLAYBACK_STATE_CHANGED);
}







