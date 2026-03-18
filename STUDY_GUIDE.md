# 0voice Qt播放器项目学习指南

## 一、项目概述

### 1.1 项目简介
这是一个基于 **Qt + FFmpeg + SDL2** 开发的跨平台视频播放器，采用 **UI层与核心层分离** 的架构设计，方便适配到PC(Win/Ubuntu/Mac)、Android、iOS等多个平台。

### 1.2 核心功能
- 播放/暂停控制
- 上一首/下一首切换
- 变速播放（支持直播低延迟播放）
- 进度条Seek定位
- 播放进度实时显示
- 视频截屏
- 音量调节
- 播放列表管理
- 缓存时间显示

### 1.3 技术栈
| 技术 | 用途 |
|------|------|
| Qt5/6 | UI界面开发 |
| FFmpeg 4.2.1 | 音视频解码、解封装 |
| SDL2 | 音频输出、多线程同步 |
| easylogging++ | 日志系统 |
| Sonic | 音频变速处理 |

---

## 二、项目架构设计

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                        UI层 (Qt)                            │
├─────────────┬─────────────┬─────────────┬───────────────────┤
│ HomeWindow  │ DisplayWind │  Playlist   │  CustomSlider     │
│  (主窗口)    │  (显示窗口)  │  (播放列表)  │   (自定义滑块)     │
└──────┬──────┴──────┬──────┴──────┬──────┴─────────┬─────────┘
│                     │                     │                    │
│ 信号槽机制            │                     │                    │
▼                     │                     │                    │
┌─────────────────────┴─────────────────────┴────────────────────┐
│                 适配层 (IjkMediaPlayer)                        │
│     将C++核心层封装成Qt友好的接口，管理播放器状态机               │
└───────────────────────────┬────────────────────────────────────┘
│                           │
│  消息队列(命令/回调)        │  视频帧回调
│                           ▼
┌───────────────────────────┴────────────────────────────────────┐
│                  核心层 (FFPlayer + Decoder)                   │
├──────────────┬──────────────┬──────────────┬───────────────────┤
│  read_thread │ audio_thread │ video_thread │video_refresh_thread│
│  (解复用线程) │ (音频解码线程)│ (视频解码线程)│   (视频刷新线程)   │
└──────────────┴──────────────┴──────────────┴───────────────────┘
│                           │
▼                           ▼
┌───────────────────┐    ┌───────────────────┐
│   FFmpeg APIs     │    │     SDL2 APIs     │
│ avformat_open_xxx │    │  SDL_OpenAudio    │
│ avcodec_decode_xxx│    │  SDL_CondWait     │
│ av_read_frame     │    │  SDL_Thread       │
└───────────────────┘    └───────────────────┘
```

### 2.2 关键类职责说明

| 类名 | 职责 | 对应文件 |
|------|------|----------|
| `HomeWindow` | 主窗口UI，处理用户交互，协调各组件 | homewindow.h/cpp |
| `IjkMediaPlayer` | 播放器对外接口，状态机管理 | ijkmediaplayer.h/cpp |
| `FFPlayer` | 核心播放逻辑，线程管理 | ff_ffplay.h/cpp |
| `Decoder` | 音视频解码器封装 | ff_ffplay.h/cpp |
| `DisplayWind` | 视频渲染显示 | displaywind.h/cpp |
| `Playlist` | 播放列表管理 | playlist.h/cpp |
| `PacketQueue` | 解封装后数据包队列 | ff_ffplay_def.h/cpp |
| `FrameQueue` | 解码后帧数据队列 | ff_ffplay_def.h/cpp |
| `MessageQueue` | 播放器消息队列(命令+事件) | ffmsg_queue.h/cpp |

---

## 三、核心数据流

### 3.1 播放启动流程

```
用户点击播放
    │
    ▼
┌──────────────────┐
│ HomeWindow::play │  1. 创建IjkMediaPlayer
│                  │  2. 设置数据源URL
└────────┬─────────┘  3. 调用prepare_async
         │
         ▼
┌────────────────────────┐
│ IjkMediaPlayer::       │  1. 创建FFPlayer实例
│   ijkmp_prepare_async  │  2. 启动消息循环线程
│                        │  3. 调用ffp_prepare_async_l
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ FFPlayer::stream_open  │  1. 初始化Frame队列
│                        │  2. 初始化Packet队列
│                        │  3. 创建read_thread
│                        │  4. 创建video_refresh_thread
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│   read_thread 执行     │  1. avformat_open_input(打开文件)
│                        │  2. avformat_find_stream_info(探测流信息)
│                        │  3. stream_component_open(打开音视频解码器)
│                        │  4. av_read_frame循环读取packet入队
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐     ┌────────────────────────┐
│ audio_thread 执行      │     │ video_thread 执行      │
│ 从audioq取packet       │     │ 从videoq取packet       │
│ ↓                     │     │ ↓                     │
│ avcodec_decode_xxx     │     │ avcodec_decode_xxx     │
│ ↓                     │     │ ↓                     │
│ 解码后的AVFrame入sampq │     │ 解码后的AVFrame入pictq │
└────────────────────────┘     └────────────────────────┘
         │                              │
         │    ┌────────────────────┐    │
         └───►│ video_refresh_thread│◄───┘
              │ 从pictq取Frame      │
              │ 调用Draw渲染        │
              │ 音频通过SDL回调输出  │
              └────────────────────┘
```

### 3.2 消息机制详解

播放器采用 **生产者-消费者模式** 的消息队列实现UI与核心的异步通信：

```
┌─────────────────────────────────────────────────────────────┐
│                      MessageQueue                          │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐  │
│  │AVMessage│───►│AVMessage│───►│AVMessage│───►│AVMessage│  │
│  │  what   │    │  what   │    │  what   │    │  what   │  │
│  │  arg1   │    │  arg1   │    │  arg1   │    │  arg1   │  │
│  │  arg2   │    │  arg2   │    │  arg2   │    │  arg2   │  │
│  └─────────┘    └─────────┘    └─────────┘    └─────────┘  │
│      ▲                                              │       │
│      │first_msg                               last_msg│       │
│      │                                              ▼       │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  SDL_mutex + SDL_cond (线程安全与同步)              │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
         ▲                                    │
         │                                    │
    ┌────┴────┐                         ┌────┴────┐
    │  生产者  │                         │  消费者  │
    │         │                         │         │
    │FFPlayer │  ffp_notify_msg_xxx     │IjkMedia │  msg_queue_get
    │内部线程  │────────────────────────►│Player   │───────────►
    │         │                         │消息循环 │
    └─────────┘                         └─────────┘
```

**消息类型分类：**

| 类型 | 宏定义 | 说明 |
|------|--------|------|
| 播放器事件 | `FFP_MSG_PREPARED` | 准备完成 |
| | `FFP_MSG_COMPLETED` | 播放完成 |
| | `FFP_MSG_SEEK_COMPLETE` | Seek完成 |
| | `FFP_MSG_BUFFERING_START/END` | 缓冲开始/结束 |
| UI请求命令 | `FFP_REQ_START` | 请求开始播放 |
| | `FFP_REQ_PAUSE` | 请求暂停 |
| | `FFP_REQ_SEEK` | 请求Seek |
| | `FFP_REQ_SCREENSHOT` | 请求截屏 |

### 3.3 音视频同步机制

```
┌─────────────────────────────────────────────────────────────┐
│                     音视频同步架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌──────────┐         ┌──────────┐         ┌──────────┐   │
│   │ audclk   │◄────────│   音频    │         │ vidclk   │   │
│   │音频时钟  │         │  主时钟   │────────►│视频时钟  │   │
│   └──────────┘         └──────────┘         └──────────┘   │
│        ▲                                        ▲          │
│        │                                        │          │
│   ┌────┴────────────────────────────────────────┴─────┐    │
│   │              get_master_clock()                   │    │
│   │         获取当前主时钟(默认音频为主)               │    │
│   └──────────────────────┬────────────────────────────┘    │
│                          │                                  │
│   ┌──────────────────────▼────────────────────────────┐    │
│   │           compute_target_delay()                  │    │
│   │    计算视频帧应该显示的时长(含同步修正)            │    │
│   │                                                   │    │
│   │    delay = 原始帧间隔 ± 同步修正值               │    │
│   │    修正值 = 视频时钟与主时钟的偏差                │    │
│   └───────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**同步策略：**
1. **音频为主时钟** (默认): 视频根据音频时钟调整显示速度
2. **视频为主时钟**: 音频根据视频时钟调整(本项目未使用)
3. **外部时钟**: 以外部参考时钟为准(本项目未使用)

---

## 四、关键数据结构

### 4.1 PacketQueue (解封装数据包队列)

```cpp
typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;  // 队首、队尾指针
    int     nb_packets;      // 包数量
    int     size;            // 总字节数
    int64_t duration;        // 总时长
    int     abort_request;   // 中止请求标志
    int     serial;          // 播放序列号(seek后+1)
    SDL_mutex *mutex;        // 互斥锁
    SDL_cond  *cond;         // 条件变量
} PacketQueue;
```

**关键设计：**
- `serial` 机制：每次seek后序列号+1，用于区分新旧数据
- `abort_request`：线程安全退出标志
- SDL锁：保证多线程安全

### 4.2 FrameQueue (解码后帧队列)

```cpp
typedef struct FrameQueue {
    Frame   queue[FRAME_QUEUE_SIZE];  // 循环队列数组
    int     rindex;         // 读索引(待播放)
    int     windex;         // 写索引(待解码)
    int     size;           // 当前帧数
    int     max_size;       // 最大容量
    int     keep_last;      // 是否保留上一帧
    int     rindex_shown;   // 读索引是否已显示
    SDL_mutex *mutex;
    SDL_cond  *cond;
    PacketQueue *pktq;      // 关联的packet队列
} FrameQueue;
```

**关键设计：**

- 循环队列：预分配内存，避免频繁malloc
- `keep_last`：保留上一帧用于某些滤镜处理
- `rindex_shown`：区分当前帧和上一帧

### 4.3 Clock (时钟系统)

```cpp
typedef struct Clock {
    double  pts;           // 当前显示帧的时间戳
    double  pts_drift;     // pts与系统时钟的差值
    double  last_updated;  // 最后更新时间
    double  speed;         // 播放速度(变速播放用)
    int     serial;        // 播放序列
    int     paused;        // 暂停标志
    int     *queue_serial; // 指向packet队列的serial
} Clock;
```

---

## 五、详细学习步骤

### 阶段一：环境熟悉与整体认知 (1-2天)

**目标：** 了解项目结构，能够编译运行

| 步骤 | 学习内容 | 相关文件 |
|------|----------|----------|
| 1.1 | 阅读README，了解项目功能 | README.md |
| 1.2 | 查看项目文件结构，理解各目录作用 | 整体目录 |
| 1.3 | 阅读.pro文件，理解编译依赖 | 0voice_player.pro |
| 1.4 | 成功编译运行项目 | Qt Creator |

**要点：**

```cpp
// main.cpp 入口分析
int main(int argc, char *argv[])
{
    INITIALIZE_EASYLOGGINGPP    // 初始化日志系统
    // ... 日志配置 ...
    QApplication a(argc, argv);
    HomeWindow w;               // 创建主窗口
    w.show();
    return a.exec();
}
```

---

### 阶段二：UI层学习 (2-3天)

**目标：** 理解Qt界面实现和用户交互处理

#### 步骤 2.1: 主窗口 HomeWindow

**阅读文件：** `homewindow.h` → `homewindow.cpp`

**关注重点：**
```cpp
// homewindow.h 关键成员
class HomeWindow : public QMainWindow {
    IjkMediaPlayer *mp_ = NULL;      // 播放器核心实例
    QTimer *play_time_ = nullptr;     // 进度更新定时器

    // 核心方法
    bool play(std::string url);       // 开始播放
    bool stop();                      // 停止播放
    int seek(int cur_valule);         // 进度跳转
    void message_loop(void *arg);     // 消息循环处理
    int OutputVideo(const Frame *frame); // 视频输出回调
};
```

**学习要点：**
1. 信号槽机制：`sig_showTips`, `sig_updateCurrentPosition` 等自定义信号
2. 播放器生命周期管理：创建 → 设置源 → 准备 → 播放 → 停止 → 销毁
3. 消息循环如何与Qt事件循环配合

#### 步骤 2.2: 显示组件 DisplayWind

**阅读文件：** `displaywind.h` → `displaywind.cpp`

**关注重点：**

```cpp
// 视频渲染流程
int DisplayWind::Draw(const Frame *frame) {
    // 1. 初始化ImageScaler(FFmpeg的SwsContext封装)
    img_scaler_->Init(video_width, video_height, frame->format,
                      img_width, img_height, AV_PIX_FMT_RGB24);

    // 2. 格式转换(YUV→RGB)
    img_scaler_->Scale3(frame, &dst_video_frame_);

    // 3. 创建QImage并触发重绘
    QImage imageTmp = QImage((uint8_t *)dst_video_frame_.data[0], ...);
    img = imageTmp.copy();
    update();  // 触发paintEvent
}

// paintEvent 实际绘制
void DisplayWind::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.drawImage(rect, img);  // 绘制到控件
}
```

#### 步骤 2.3: 播放列表 Playlist

**阅读文件：** `playlist.h` → `playlist.cpp`

**学习要点：**

- 文件拖拽支持：`dropEvent`, `dragEnterEvent`
- 播放列表持久化：`savePlayList`
- 双击播放信号：`SigPlay`



在析构playlist时或OnAddFile成功时，会调用savePlayList();保存当前的播放列表和音量到配置文件player_config.ini

OnAddFile会做前缀和后缀匹配。

双击文件名会触发SigPlay()信号，调用play()播放。



---

### 阶段三：适配层学习 (2-3天)

**目标：** 理解 IjkMediaPlayer 如何封装FFPlayer

#### 步骤 3.1: 播放器状态机

**阅读文件：** `ijkmediaplayer.h` (第24-111行状态定义)

**状态流转图：**

```
IDLE ──set_data_source──► INITIALIZED ──prepare_async──► ASYNC_PREPARING
                                                              │
                                                              ▼
ERROR ◄────────────────────────────────────────────────── PREPARED ◄───发现流信息
   ▲                                                           │
   │                                                           │ start
   │                                                           ▼
   │◄───────────────── stop ─────────────────────────────── STARTED
   │                                                            │
   │                                                            │ pause
   │                                                            ▼
   │◄───────────────── stop ──────────────────────────────── PAUSED
   │                                                            │
   │                                                            │ seek
   │                                                            ▼
   │◄───────────────── stop ──────────────────────────── COMPLETED
   │
   └── 任何阶段出错都会进入ERROR状态
```

#### 步骤 3.2: 消息分发机制

**阅读文件：** `ijkmediaplayer.cpp` (第147-214行 `ijkmp_get_msg`)

```cpp
// 消息处理主循环
int IjkMediaPlayer::ijkmp_get_msg(AVMessage *msg, int block) {
    while (1) {
        int retval = msg_queue_get(&ffplayer_->msg_queue_, msg, block);

        switch (msg->what) {
            case FFP_MSG_PREPARED:
                mp->ijkmp_start();  // 准备完成后自动播放
                break;

            case FFP_REQ_START:
                ffplayer_->ffp_start_l();
                ijkmp_change_state_l(MP_STATE_STARTED);
                break;

            case FFP_REQ_PAUSE:
                ffplayer_->ffp_pause_l();
                ijkmp_change_state_l(MP_STATE_PAUSED);
                break;

            case FFP_REQ_SEEK:
                ffplayer_->ffp_seek_to_l(msg->arg1);
                break;

            // ... 其他消息处理
        }
    }
}
```

---

### 阶段四：核心层学习 (5-7天)

**目标：** 深入理解FFmpeg播放原理

#### 步骤 4.1: 数据结构与队列系统

**阅读文件：** `ff_ffplay_def.h` → `ff_ffplay_def.cpp`

**学习顺序：**
1. `PacketQueue` 实现 (包队列操作)
2. `FrameQueue` 实现 (帧队列操作)
3. `Clock` 实现 (时钟系统)

**重点函数分析：**
```cpp
// PacketQueue 核心操作
int packet_queue_put(PacketQueue *q, AVPacket *pkt);  // 入队
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial); // 出队
void packet_queue_start(PacketQueue *q);  // 启动，放入flush_pkt

// FrameQueue 核心操作
Frame *frame_queue_peek_writable(FrameQueue *f);  // 获取可写位置
Frame *frame_queue_peek_readable(FrameQueue *f);  // 获取可读帧
void frame_queue_push(FrameQueue *f);  // 写入完成，更新写索引
void frame_queue_next(FrameQueue *f);  // 读取完成，更新读索引
```

#### 步骤 4.2: 解复用线程 read_thread

**阅读文件：** `ff_ffplay.cpp` 中 `FFPlayer::read_thread`

**流程分析：**
```cpp
int FFPlayer::read_thread() {
    // 1. 打开输入文件
    avformat_open_input(&ic, filename, ...);

    // 2. 探测流信息
    avformat_find_stream_info(ic, ...);

    // 3. 查找音视频流索引
    video_stream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, ...);
    audio_stream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, ...);

    // 4. 打开解码器
    stream_component_open(video_stream);
    stream_component_open(audio_stream);

    // 5. 读取packet循环
    while (!abort_request) {
        // 处理seek请求
        if (seek_req) {
            av_seek_frame(ic, ..., seek_pos, ...);
        }

        // 控制队列大小，避免内存暴涨
        if (audioq.size + videoq.size > MAX_QUEUE_SIZE) {
            SDL_Delay(10);
            continue;
        }

        // 读取packet
        ret = av_read_frame(ic, pkt);

        // 根据类型放入对应队列
        if (pkt->stream_index == video_stream)
            packet_queue_put(&videoq, pkt);
        else if (pkt->stream_index == audio_stream)
            packet_queue_put(&audioq, pkt);
    }
}
```

#### 步骤 4.3: 解码线程 (audio_thread/video_thread)

**阅读文件：** `ff_ffplay.cpp` 中 `Decoder::audio_thread` 和 `video_thread`

**音频解码流程：**
```cpp
int Decoder::audio_thread(void* arg) {
    while (!abort) {
        // 1. 从队列取packet
        packet_queue_get(queue_, &pkt, 1, &serial);

        // 2. 发送给解码器
        avcodec_send_packet(avctx_, &pkt);

        // 3. 循环接收解码后的frame
        while (ret >= 0) {
            ret = avcodec_receive_frame(avctx_, frame);

            // 4. 获取可写Frame位置
            Frame *af = frame_queue_peek_writable(&sampq);

            // 5. 填充frame信息
            af->pts = ...;
            af->frame = frame;
            af->serial = serial;

            // 6. 更新写索引
            frame_queue_push(&sampq);
        }
    }
}
```

#### 步骤 4.4: 视频刷新与同步

**阅读文件：** `ff_ffplay.cpp` 中 `video_refresh_thread` 和 `video_refresh`

**同步算法核心：**
```cpp
void FFPlayer::video_refresh(double *remaining_time) {
    // 1. 获取当前要显示的帧
    Frame *vp = frame_queue_peek(&pictq);
    Frame *nextvp = frame_queue_peek_next(&pictq);

    // 2. 计算当前帧应该显示的时长
    double delay = vp_duration(vp, nextvp);

    // 3. 根据主时钟计算同步修正
    double diff = get_clock(&vidclk) - get_master_clock();

    // 4. 同步策略
    if (diff <= -sync_threshold) {
        // 视频落后，缩短显示时间(或立即显示下一帧)
        delay = FFMAX(0, delay + diff);
    } else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {
        // 视频超前，延长显示时间(或丢弃帧)
        delay = delay + diff;
    } else if (diff >= sync_threshold) {
        // 双倍延迟
        delay = 2 * delay;
    }

    // 5. 等待指定时间后显示
    *remaining_time = FFMIN(*remaining_time, delay);

    // 6. 回调UI显示
    video_refresh_callback_(vp);

    // 7. 更新视频时钟
    update_video_pts(vp->pts, vp->pos, vp->serial);

    // 8. 更新读索引
    frame_queue_next(&pictq);
}
```

#### 步骤 4.5: 音频输出与重采样

**阅读文件：** `ff_ffplay.cpp` 中 `audio_open`, `audio_decode_frame`

**关键流程：**
```cpp
// 1. 打开音频设备
int FFPlayer::audio_open(...) {
    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.freq = sample_rate;      // 采样率
    wanted_spec.format = AUDIO_S16SYS;   // 格式
    wanted_spec.channels = channels;     // 通道数
    wanted_spec.callback = sdl_audio_callback;  // 回调函数

    SDL_OpenAudio(&wanted_spec, &spec);
}

// 2. SDL音频回调(需要持续提供数据)
void sdl_audio_callback(void *opaque, Uint8 *stream, int len) {
    // 调用 audio_decode_frame 获取解码后的PCM数据
    int audio_size = audio_decode_frame(is);

    // 拷贝到SDL缓冲区
    memcpy(stream, is->audio_buf, audio_size);
}

// 3. 音频重采样(格式转换)
if (af->frame->format != audio_src.fmt ||
    dec_channel_layout != audio_src.channel_layout) {
    // 创建SwrContext
    swr_ctx = swr_alloc_set_opts(...);
    swr_init(swr_ctx);

    // 执行重采样
    len2 = swr_convert(swr_ctx, out, out_count, in, in_samples);
}
```

#### 步骤 4.6: Seek实现

**阅读文件：** `ff_ffplay.cpp` 中 `ffp_seek_to_l`, `stream_seek`

**Seek流程：**
```cpp
int FFPlayer::ffp_seek_to_l(long msec) {
    // 1. 计算seek位置
    int64_t seek_pos = msec * AV_TIME_BASE / 1000;

    // 2. 设置seek标志
    seek_req = 1;
    seek_pos = seek_pos;
    seek_flags = seek_by_bytes ? AVSEEK_FLAG_BYTE : AVSEEK_FLAG_BACKWARD;

    // 3. read_thread检测到seek_req后执行
    av_seek_frame(ic, -1, seek_pos, seek_flags);

    // 4. 清空队列，插入flush_pkt(序列号+1)
    packet_queue_flush(&audioq);
    packet_queue_flush(&videoq);
    packet_queue_put(&audioq, &flush_pkt);
    packet_queue_put(&videoq, &flush_pkt);

    // 5. 解码器线程检测到serial变化，会丢弃旧数据
}
```

---

### 阶段五：高级功能学习 (3-4天)

#### 步骤 5.1: 变速播放

**阅读文件：** `sonic.h/cpp`, `ff_ffplay.cpp` 中变速相关代码

**原理：**
```cpp
// 使用Sonic库进行变速不变调处理
void ffp_set_playback_rate(float rate) {
    pf_playback_rate = rate;
    pf_playback_rate_changed = 1;
}

// 音频解码时处理
if (pf_playback_rate != 1.0) {
    // 通过sonic变速
    sonicWriteFloatToStream(audio_speed_convert, ...);
    int ret = sonicReadFloatFromStream(audio_speed_convert, ...);
}
```

#### 步骤 5.2: 截屏功能

**阅读文件：** `screenshot.h/cpp`

```cpp
int ffp_screenshot_l(char *screen_path) {
    req_screenshot_ = true;
    screen_path_ = strdup(screen_path);
}

// video_refresh线程中检测截屏请求
void FFPlayer::screenshot(AVFrame *frame) {
    // 使用sws_scale转换格式
    // 使用libjpeg保存为JPG
}
```

#### 步骤 5.3: 直播低延迟优化

**阅读文件：** `homewindow.cpp` 中变速播放触发逻辑

```cpp
// 实时流检测
if (is_realtime(url)) {  // rtsp/rtmp/rtp/udp
    // 缓存过大时加速播放
    if (audio_cache_duration > max_cache_duration_ + network_jitter_duration_) {
        mp_->ijkmp_set_playback_rate(1.2);  // 加速
    }
    // 缓存正常后恢复
    if (is_accelerate_speed_ && audio_cache_duration < max_cache_duration_) {
        mp_->ijkmp_set_playback_rate(1.0);  // 恢复
    }
}
```

---

## 六、调试技巧

### 6.1 日志系统使用

```cpp
// easylogging++ 使用
LOG(TRACE) << "详细跟踪信息";
LOG(DEBUG) << "调试信息";
LOG(INFO) << "一般信息";
LOG(WARNING) << "警告信息";
LOG(ERROR) << "错误信息";

// 查看日志文件: log/log_YYYYMMDD.log
```

### 6.2 关键调试断点

| 文件 | 函数 | 调试目的 |
|------|------|----------|
| ff_ffplay.cpp | `read_thread` | 查看解复用流程 |
| ff_ffplay.cpp | `video_refresh` | 查看同步逻辑 |
| homewindow.cpp | `message_loop` | 查看消息分发 |
| ijkmediaplayer.cpp | `ijkmp_get_msg` | 查看状态转换 |

### 6.3 常见问题排查

**问题1: 有声音无画面**
- 检查 `video_refresh_thread` 是否运行
- 检查 `OutputVideo` 回调是否被调用
- 检查 `DisplayWind::Draw` 是否正常

**问题2: 画面卡顿**
- 检查 `videoq` 和 `pictq` 的size
- 检查音视频同步 diff 值
- 检查是否触发 framedrop

**问题3: Seek后花屏**
- 检查 flush_pkt 是否正确插入
- 检查解码器是否正确刷新
- 检查 serial 是否正确更新

---

## 七、扩展学习建议

### 7.1 FFmpeg官方示例
- `ffplay.c`: 本项目的原始参考代码
- `doc/examples`: FFmpeg官方示例程序

### 7.2 推荐阅读
1. **《FFmpeg从入门到精通》**: 系统学习FFmpeg API
2. **《音视频开发进阶指南》**: 深入理解音视频原理
3. **雷霄骅的博客**: 经典FFmpeg教程

### 7.3 进阶方向
1. **硬件解码**: 集成VideoToolbox/ MediaCodec/ DXVA
2. **滤镜系统**: 集成FFmpeg滤镜图
3. **字幕支持**: 添加ASS/SRT字幕渲染
4. **多音轨切换**: 支持音频流动态切换

---

## 八、学习检查清单

### 基础理解
- [ ] 能画出项目架构图
- [ ] 能解释播放启动的完整流程
- [ ] 能理解消息队列的作用
- [ ] 能解释PacketQueue和FrameQueue的区别

### 核心掌握
- [ ] 能解释音视频同步原理
- [ ] 能理解serial机制的作用
- [ ] 能解释seek的实现流程
- [ ] 能理解变速播放的原理

### 代码实践
- [ ] 能独立添加一个新的UI控件
- [ ] 能修改消息类型并正确处理
- [ ] 能添加一个新的播放器功能
- [ ] 能定位和修复常见播放问题

---

**祝学习愉快！遇到问题多结合代码调试，理解会更深刻。**





播放：
播放列表双击播放 → play() 函数
选择本地视频时——>play()函数启动播放
上一个/下一个 play(url);
点击播放按钮：如果当前不存在播放，则play()新建播放，当前为暂停-->改变播放/暂停状态

```
play()
// 如果本身处于播放状态则先停止原有的播放
    if(mp_) {
        stop();
    }
// 1. 先检测mp是否已经创建
mp_ = new IjkMediaPlayer();
//1.1 创建
    ret = mp_->ijkmp_create(std::bind(&HomeWindow::message_loop, this, std::placeholders::_1));
//添加视频回调
 mp_->AddVideoRefreshCallback(std::bind(&HomeWindow::OutputVideo, this,
                                           std::placeholders::_1));
// 1.2 设置url
    mp_->ijkmp_set_data_source(url.c_str());
    mp_->ijkmp_set_playback_volume(ui->volumeSlider->value());
// 1.3 准备工作
    ret = mp_->ijkmp_prepare_async();
    ui->display->StartPlay();
```

停止：stop()

点击停止按钮
处于播放状态时更换播放会先触发stop()
播放完毕会触发sig_stopped信号--》stop()

```
stop()
bool HomeWindow::stop()
{
    if(mp_) {
        stopTimer();
        mp_->ijkmp_stop();  //传入底层播放核心层ffplayer_->ffp_stop_l(); 设置退出标识
        mp_->ijkmp_destroy(); //摧毁一系列线程、队列等
        delete mp_;
        mp_ = NULL;
        real_time_ = 0;
        is_accelerate_speed_ = false;
        ui->display->StopPlay();        // 停止渲染，后续刷黑屏
        ui->playOrPauseBtn->setText("播放");
        return 0;
    } else {
        return -1;
    }
}
```





快进/快退

点击快进/快退按钮，调用函数 fastForward(MP_SEEK_STEP);和fastBack(-1 * MP_SEEK_STEP);

MP_SEEK_STEP为10秒 表示步长

ui层 fastForward-》控制层mp_->ijkmp_forward_to(inrc);-》播放器核心层 消息队列发送FFP_REQ_FORWARD

消息循环收到FFP_REQ_FORWARD后，ffplayer_->ffp_forward_to_l(msg->arg1);-》ffp_forward_or_back_to_l(incr);-》stream_seek 修改seek_seq标识



上一个/下一个

on_prevBtn_clicked()/on_nextBtn_clicked()

// 停止当前的播放，然后播放下一个，这里就需要播放列表配合

//获取前一个播放的url 并将对应的url选中
如果url不为空，play(url);  否则提示 “没有可以播放的URL”



变速

点击倍速按钮   

// 先获取当前的倍速,每次叠加0.5, 支持0.5~2.0倍速  float rate =  mp_->ijkmp_get_playback_rate() + 0.5;

mp_->ijkmp_set_playback_rate(rate); -》 ffplayer_->ffp_set_playback_rate(rate);

设置标识

```
pf_playback_rate = rate;
pf_playback_rate_changed = 1;
```



seek操作

点击或滑动播放进度条，捕捉到鼠标事件，获取到改变量value，on_playSliderValueChanged-》seek(value)-》设置标识 req_seeking_ = true; 向消息队列发送FFP_REQ_SEEK
捕捉到FFP_REQ_SEEK消息后，fplayer_->ffp_seek_to_l(msg->arg1); -》stream_seek(seek_pos, 0, 0); 设置seek标识



音量调节

点击或滑动音量条，捕捉到鼠标事件，获取到改变量value，mp_->ijkmp_set_playback_volume(value);-》ffplayer_->ffp_set_playback_volume(volume); 将音量设置audio_volume = value;





## homewindow.h

● 这是 Qt 视频播放器的主窗口头文件，定义了 HomeWindow 类的接口。以下是详细解释：                                                                                                                                                                                                                                                                                                    
  头文件保护                                                                                                                                                                              
                                                                                                                                                                                            #ifndef HOMEWINDOW_H                                                                                                                                                                    
  #define HOMEWINDOW_H                                                                                                                                                                    
  ...
  #endif // HOMEWINDOW_H
  防止重复包含。

  前置声明

  namespace Ui {
  class HomeWindow;  // Qt Designer 生成的 UI 类
  }

  类定义

  class HomeWindow : public QMainWindow
  {
      Q_OBJECT  // 启用 Qt 元对象系统（信号槽、RTTI 等）

  公共方法

  ┌─────────────────────────────────┬────────────────────────────────┐
  │              方法               │              说明              │
  ├─────────────────────────────────┼────────────────────────────────┤
  │ HomeWindow(QWidget *parent = 0) │ 构造函数                       │
  ├─────────────────────────────────┼────────────────────────────────┤
  │ ~HomeWindow()                   │ 析构函数                       │
  ├─────────────────────────────────┼────────────────────────────────┤
  │ initUi()                        │ 初始化界面                     │
  ├─────────────────────────────────┼────────────────────────────────┤
  │ InitSignalsAndSlots()           │ 初始化信号槽连接               │
  ├─────────────────────────────────┼────────────────────────────────┤
  │ message_loop(void *arg)         │ 消息循环（用于接收播放器事件） │
  ├─────────────────────────────────┼────────────────────────────────┤
  │ OutputVideo(const Frame *frame) │ 视频帧输出回调                 │
  └─────────────────────────────────┴────────────────────────────────┘

  重写的事件方法

  void resizeEvent(QResizeEvent *event);  // 窗口大小变化事件
  void resizeUI();                         // 调整 UI 布局

  信号 (Signals)

  ┌───────────────────────────────────────┬───────────────────┐
  │                 信号                  │       说明        │
  ├───────────────────────────────────────┼───────────────────┤
  │ sig_showTips(Toast::Level, QString)   │ 显示提示信息      │
  ├───────────────────────────────────────┼───────────────────┤
  │ sig_updateAudioCacheDuration(int64_t) │ 更新音频缓存时长  │
  ├───────────────────────────────────────┼───────────────────┤
  │ sig_updateVideoCacheDuration(int64_t) │ 更新视频缓存时长  │
  ├───────────────────────────────────────┼───────────────────┤
  │ sig_updateCurrentPosition(long)       │ 更新当前播放位置  │
  ├───────────────────────────────────────┼───────────────────┤
  │ sig_updatePlayOrPause(int)            │ 更新播放/暂停状态 │
  ├───────────────────────────────────────┼───────────────────┤
  │ sig_stopped()                         │ 被动停止信号      │
  └───────────────────────────────────────┴───────────────────┘

  槽函数 (Slots)

  文件操作
  - on_openFile() - 打开本地文件
  - on_openNetworkUrl() - 打开网络流

  播放控制
  - on_playOrPauseBtn_clicked() - 播放/暂停
  - on_stopBtn_clicked() - 停止
  - on_prevBtn_clicked() / on_nextBtn_clicked() - 上一首/下一首
  - on_forwardFastBtn_clicked() / on_backFastBtn_clicked() - 快进/快退

  UI 响应
  - on_playSliderValueChanged(int) - 进度条拖动
  - on_volumeSliderValueChanged(int) - 音量调节
  - on_speedBtn_clicked() - 倍速播放
  - on_screenBtn_clicked() - 全屏切换
  - on_listBtn_clicked() - 显示/隐藏播放列表

  配置
  - on_bufDurationBox_currentIndexChanged() - 缓存时长设置
  - on_jitterBufBox_currentIndexChanged() - 网络抖动缓冲设置

  私有方法

  ┌──────────────────────────────────────────────┬──────────────────┐
  │                     方法                     │       功能       │
  ├──────────────────────────────────────────────┼──────────────────┤
  │ play(std::string url)                        │ 开始播放指定 URL │
  ├──────────────────────────────────────────────┼──────────────────┤
  │ stop() / pause() / resume()                  │ 播放状态控制     │
  ├──────────────────────────────────────────────┼──────────────────┤
  │ seek(int)                                    │ 跳转到指定位置   │
  ├──────────────────────────────────────────────┼──────────────────┤
  │ fastForward(long) / fastBack(long)           │ 快进/快退        │
  ├──────────────────────────────────────────────┼──────────────────┤
  │ startTimer() / stopTimer()                   │ 定时器控制       │
  ├──────────────────────────────────────────────┼──────────────────┤
  │ resizeCtrlBar() / resizeDisplayAndFileList() │ 界面布局调整     │
  └──────────────────────────────────────────────┴──────────────────┘

  成员变量

  Ui::HomeWindow *ui;           // UI 对象
  IjkMediaPlayer *mp_ = NULL;   // ijkplayer 播放器核心实例

  // 播放状态
  long total_duration_ = 0;     // 总时长(ms)
  long current_position_ = 0;   // 当前位置
  bool req_seeking_ = false;    // 是否正在 seek

  // 缓存配置
  int max_cache_duration_ = 400;      // 最大缓存 400ms
  int network_jitter_duration_ = 100; // 网络抖动缓冲 100ms
  float accelerate_speed_factor_ = 1.2; // 加速倍率
  float normal_speed_factor_ = 1.0;     // 正常倍率

  // 缓存统计
  int64_t audio_cache_duration = 0;
  int64_t video_cache_duration = 0;

  QTimer *play_time_ = nullptr;  // 播放进度更新定时器
  bool is_show_file_list_ = true; // 播放列表显示状态

  设计特点：
  - 使用 ijkplayer 作为底层播放引擎
  - 通过信号槽机制实现 UI 与播放逻辑的解耦
  - 支持本地文件和网络流播放
  - 具备缓存管理和抗网络抖动能力





homewindow.cpp

● 这是 Qt 视频播放器主窗口的实现文件，基于 ijkplayer 内核。以下是详细解释：                                                                                                                                                                                                                                                                                                         
  工具函数                                                                                                                                                                                
                                                                                                                                                                                            int64_t get_ms()  // 获取当前毫秒时间戳                                                                                                                                                 
  static int is_realtime(const char *url)  // 判断是否为直播流（rtp/rtsp/udp/rtmp等）                                                                                                     

  构造函数 (homewindow.cpp:39-57)

  HomeWindow::HomeWindow(QWidget *parent) :
      QMainWindow(parent),
      ui(new Ui::HomeWindow)
  {
      ui->setupUi(this);
      ui->playList->Init();
      // 初始化时间显示为 00:00:00
      // 初始化缓存显示为 0ms
      // 设置默认缓存参数：400ms 缓存，100ms 抖动缓冲
      initUi();           // 加载样式表
      InitSignalsAndSlots();  // 绑定信号槽
  }

  信号槽初始化 (homewindow.cpp:69-97)

  核心连接包括：
  - 播放列表双击播放 → play() 函数
  - 进度条/音量条拖动 → 对应处理函数
  - 注册自定义类型 Toast::Level 和 int64_t（用于跨线程信号）
  - 菜单栏打开文件/URL 动作

  消息循环 - 核心 (homewindow.cpp:99-172)

  int HomeWindow::message_loop(void *arg)
  在独立线程中运行，处理播放器事件：

  ┌────────────────────────────────┬─────────────────────────────┐
  │            消息类型            │            处理             │
  ├────────────────────────────────┼─────────────────────────────┤
  │ FFP_MSG_PREPARED               │ 准备完成，开始播放          │
  ├────────────────────────────────┼─────────────────────────────┤
  │ FFP_MSG_FIND_STREAM_INFO       │ 获取媒体信息，更新总时长    │
  ├────────────────────────────────┼─────────────────────────────┤
  │ FFP_MSG_PLAYBACK_STATE_CHANGED │ 播放状态变化，更新 UI       │
  ├────────────────────────────────┼─────────────────────────────┤
  │ FFP_MSG_SEEK_COMPLETE          │ 跳转完成，重置 seeking 标志 │
  ├────────────────────────────────┼─────────────────────────────┤
  │ FFP_MSG_SCREENSHOT_COMPLETE    │ 截图完成，显示提示          │
  ├────────────────────────────────┼─────────────────────────────┤
  │ FFP_MSG_PLAY_FNISH             │ 播放完毕，触发停止          │
  └────────────────────────────────┴─────────────────────────────┘

  循环逻辑：
  1. 每 20ms 获取一次消息
  2. 每 500ms 更新缓存时长和播放位置
  3. 处理直播流的变速播放逻辑

  视频渲染 (homewindow.cpp:174-178)

  int HomeWindow::OutputVideo(const Frame *frame)
  {
      return ui->display->Draw(frame);  // 将视频帧传给显示控件
  }

  UI 布局调整 (homewindow.cpp:180-230)

  void HomeWindow::resizeUI()
  窗口大小变化时调整：

  - 控制栏位置（底部）
  - 播放列表按钮和设置按钮位置
  - 视频显示区域大小
  - 进度条和音量条位置

  播放控制功能

  打开文件/URL

  void on_openFile()         // 弹出文件对话框，选择本地视频
  void on_openNetworkUrl()   // 弹出对话框输入网络流地址

  播放流程 (homewindow.cpp:427-462)

  bool HomeWindow::play(std::string url)
  执行步骤：
  1. 停止当前播放
  2. 检测是否为直播流
  3. 创建 IjkMediaPlayer 实例
  4. 设置消息循环回调和视频刷新回调
  5. 设置数据源和音量
  6. 异步准备并开始播放
  7. 启动定时器

  停止流程 (homewindow.cpp:520-536)

  bool HomeWindow::stop()
  - 停止并销毁播放器实例
  - 停止渲染（刷黑屏）
  - 重置状态和 UI

  播放/暂停切换 (homewindow.cpp:387-408)

  void on_playOrPauseBtn_clicked()
  - 未初始化时：播放播放列表当前项
  - 播放中：暂停
  - 暂停中：恢复播放

  进度控制

  跳转 (homewindow.cpp:283-296)

  int HomeWindow::seek(int cur_valule)
  将进度条值（0-6000）转换为时间百分比，调用 ijkmp_seek_to

  快进/快退 (homewindow.cpp:298-316)

  int fastForward(long inrc)   // 前进指定毫秒
  int fastBack(long inrc)      // 后退指定毫秒

  缓存管理与变速播放 (homewindow.cpp:349-373)

  void HomeWindow::reqUpdateCacheDuration()

  直播流变速策略：
  - 当音频缓存 > max_cache_duration_ + network_jitter_duration_（默认 500ms）时，开启 1.2 倍速播放
  - 当缓存恢复正常（< 400ms）时，恢复 1.0 倍速
  - 目的：消除直播累积延迟，追赶实时位置

  播放信息显示

  更新当前位置 (homewindow.cpp:334-347)

  每 500ms 获取一次当前播放位置（非 seek 状态下），转换为 HH:MM:SS 格式显示

  更新总时长 (homewindow.cpp:318-332)

  媒体信息获取后，将总毫秒转换为 HH:MM:SS 格式

  缓存时长显示

  实时显示音频/视频缓存时长（毫秒）

  其他功能

  ┌───────────────┬─────────────────────────────────────────────────────────┐
  │     功能      │                          实现                           │
  ├───────────────┼─────────────────────────────────────────────────────────┤
  │ 倍速播放      │ on_speedBtn_clicked() - 0.5~2.0 倍速循环切换            │
  ├───────────────┼─────────────────────────────────────────────────────────┤
  │ 全屏截图      │ on_screenBtn_clicked() - 保存为 yyyyMMdd-hhmmss-zzz.jpg │
  ├───────────────┼─────────────────────────────────────────────────────────┤
  │ 播放列表显隐  │ on_listBtn_clicked() - 切换显示并重新布局               │
  ├───────────────┼─────────────────────────────────────────────────────────┤
  │ 上一首/下一首 │ on_prevBtn_clicked() / on_nextBtn_clicked()             │
  ├───────────────┼─────────────────────────────────────────────────────────┤
  │ 缓存配置      │ 下拉框选择 30ms~4000ms 不同缓存档位                     │
  └───────────────┴─────────────────────────────────────────────────────────┘

  状态管理

  关键成员变量作用：
  bool req_seeking_          // 是否正在跳转（避免进度条抖动）
  bool is_accelerate_speed_  // 是否处于加速播放状态
  int real_time_             // 是否为实时流（直播）
  int64_t total_duration_    // 媒体总时长
  int64_t current_position_  // 当前播放位置

  执行流程图：
  用户点击播放 → play() → 创建 IjkMediaPlayer → 设置数据源 → prepare_async()
                      ↓
                消息循环线程启动
                      ↓
      FFP_MSG_PREPARED → ijkmp_start() → 开始播放
                      ↓
      定时更新位置 ← 消息循环 ← 视频帧回调 → 渲染显示