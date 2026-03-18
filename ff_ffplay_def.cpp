#include "ff_ffplay_def.h"
#include "easylogging++.h"

AVPacket flush_pkt;
static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;

    if (q->abort_request)   //如果已中止，则放入失败
        return -1;

    pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));   //分配节点内存
    if (!pkt1)  //内存不足，则放入失败
        return -1;
    // 没有做引用计数，那这里也说明av_read_frame不会释放替用户释放buffer。
    pkt1->pkt = *pkt; //拷贝AVPacket(浅拷贝，AVPacket.data等内存并没有拷贝)
    pkt1->next = NULL;
    if (pkt == &flush_pkt)//如果放入的是flush_pkt，需要增加队列的播放序列号，以区分不连续的两段数据
    {
        q->serial++;
        LOG(INFO) << "q->serial = " << q->serial;
    }
    pkt1->serial = q->serial;   //用队列序列号标记节点
    /* 队列操作：如果last_pkt为空，说明队列是空的，新增节点为队头；
     * 否则，队列有数据，则让原队尾的next为新增节点。 最后将队尾指向新增节点
     */
    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;

    //队列属性操作：增加节点数、cache大小、cache总时长, 用来控制队列的大小
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    q->duration += pkt1->pkt.duration;

    /* XXX: should duplicate packet data in DV case */
    //发出信号，表明当前队列中有数据了，通知等待中的读线程可以取数据了
    SDL_CondSignal(q->cond);
    return 0;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt);//主要实现
    SDL_UnlockMutex(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_packet_unref(pkt);       //放入失败，释放AVPacket

    return ret;
}

int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

/* packet queue handling */
int packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    if (!q->mutex) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if (!q->cond) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    q->abort_request = 1;
    return 0;
}

void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    SDL_UnlockMutex(q->mutex);
}

void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q); //先清除所有的节点
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;       // 请求退出

    SDL_CondSignal(q->cond);    //释放一个条件信号

    SDL_UnlockMutex(q->mutex);
}

void packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt); //这里放入了一个flush_pkt
    SDL_UnlockMutex(q->mutex);
}

double packet_queue_cache_duration(PacketQueue *q, AVRational time_base, double packet_duration)
{
    double pts_duration = 0;    // 按队列头部、尾部的pts时长
    double packets_duration = 0;// 按包累积的时长

    SDL_LockMutex(q->mutex);
    MyAVPacketList	*first_pkt = q->first_pkt;
    MyAVPacketList  *last_pkt  = q->last_pkt;
    int64_t temp_pts = last_pkt->pkt.dts - first_pkt->pkt.dts;  // 计算出来差距
    pts_duration = temp_pts  * av_q2d(time_base);            // 转换成秒

    packets_duration = packet_duration * q->nb_packets; // packet可能存在多帧音频的情况, 此时这里计算就存在误差

    SDL_UnlockMutex(q->mutex);

    // 以时间戳为准，然后根据码率预估持续播放的最大时长，比如以时间戳计算出来超过60秒，则是极有可能是有问题的,说明缓存的数据比较多
    if(pts_duration < 60) {
        return pts_duration;
    } else {
        return packets_duration;
    }
}


/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
/**
 * @brief packet_queue_get
 * @param q 队列
 * @param pkt 输出参数，即MyAVPacketList.pkt
 * @param block 调用者是否需要在没节点可取的情况下阻塞等待
 * @param serial 输出参数，即MyAVPacketList.serial
 * @return <0: aborted; =0: no packet; >0: has packet
 */
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);    // 加锁

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;    //MyAVPacketList *pkt1; 从队头拿数据
        if (pkt1) {     //队列中有数据
            q->first_pkt = pkt1->next;  //队头移到第二个节点
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;    //节点数减1
            q->size -= pkt1->pkt.size + sizeof(*pkt1);  //cache大小扣除一个节点
            q->duration -= pkt1->pkt.duration;  //总时长扣除一个节点
            //返回AVPacket，这里发生一次AVPacket结构体拷贝，AVPacket的data只拷贝了指针
            *pkt = pkt1->pkt;
            if (serial) //如果需要输出serial，把serial输出
                *serial = pkt1->serial;
            av_free(pkt1);      //释放节点内存,只是释放节点，而不是释放AVPacket
            ret = 1;
            break;
        } else if (!block) {    //队列中没有数据，且非阻塞调用
            ret = 0;
            break;
        } else {    //队列中没有数据，且阻塞调用
            //这里没有break。for循环的另一个作用是在条件变量满足后重复上述代码取出节点
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);  // 释放锁
    return ret;
}



static void frame_queue_unref_item(Frame *vp)
{
    av_frame_unref(vp->frame);	/* 释放数据 */
}

/* 初始化FrameQueue，视频和音频keep_last设置为1，字幕设置为0 */
int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last)
{
    int i;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    if (!(f->cond = SDL_CreateCond())) {
        av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc())) // 分配AVFrame结构体
            return AVERROR(ENOMEM);
    return 0;
}

void frame_queue_destory(FrameQueue *f)
{
    int i;
    for (i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        // 释放对vp->frame中的数据缓冲区的引用，注意不是释放frame对象本身
        frame_queue_unref_item(vp);
        // 释放vp->frame对象
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(f->mutex);
    SDL_DestroyCond(f->cond);
}

void frame_queue_signal(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

/* 获取队列当前Frame, 在调用该函数前先调用frame_queue_nb_remaining确保有frame可读 */
Frame *frame_queue_peek(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

/* 获取当前Frame的下一Frame, 此时要确保queue里面至少有2个Frame */
// 不管你什么时候调用，返回来肯定不是 NULL
Frame *frame_queue_peek_next(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

/* 获取last Frame：
 */
Frame *frame_queue_peek_last(FrameQueue *f)
{
    return &f->queue[f->rindex];
}
// 获取可写指针
Frame *frame_queue_peek_writable(FrameQueue *f)
{
    /* wait until we have space to put a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size &&
           !f->pktq->abort_request) {	/* 检查是否需要退出 */
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)			 /* 检查是不是要退出 */
        return NULL;

    return &f->queue[f->windex];
}
// 获取可读
Frame *frame_queue_peek_readable(FrameQueue *f)
{
    /* wait until we have a readable a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size <= 0 &&
           !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}
// 更新写指针
void frame_queue_push(FrameQueue *f)
{
    if (++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size++;
    SDL_CondSignal(f->cond);    // 当_readable在等待时则可以唤醒
    SDL_UnlockMutex(f->mutex);
}

/* 释放当前frame，并更新读索引rindex */
void frame_queue_next(FrameQueue *f)
{
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    SDL_LockMutex(f->mutex);
    f->size--;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

/* return the number of undisplayed frames in the queue */
int frame_queue_nb_remaining(FrameQueue *f)
{
    return f->size - f->rindex_shown;
}

/* return last shown position */
int64_t frame_queue_last_pos(FrameQueue *f)
{
    Frame *fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == f->pktq->serial)
        if(fp)
            return fp->pos;
        else
            return -1;
}


/**
 * 获取到的实际上是:最后一帧的pts 加上 从处理最后一帧开始到现在的时间,具体参考set_clock_at 和get_clock的代码
 * c->pts_drift=最后一帧的pts-从处理最后一帧时间
 * clock=c->pts_drift+现在的时候
 * get_clock(&is->vidclk) ==is->vidclk.pts, av_gettime_relative() / 1000000.0 -is->vidclk.last_updated  +is->vidclk.pts
 */
double get_clock(Clock *c)
{
    if (*c->queue_serial != c->serial)
        return NAN; // 不是同一个播放序列，时钟是无效
    if (c->paused) {
        return c->pts;  // 暂停的时候返回的是pts
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

void set_clock_at(Clock *c, double pts, int serial,double time)
{
    c->pts		= pts;                      /* 当前帧的pts */
    c->last_updated = time;                 /* 最后更新的时间，实际上是当前的一个系统时间 */
    c->pts_drift	= c->pts - time;        /* 当前帧pts和系统时间的差值，正常播放情况下两者的差值应该是比较固定的，因为两者都是以时间为基准进行线性增长 */
    c->serial = serial;
}

void set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}


void init_clock(Clock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}


void ffp_reset_statistic(FFStatistic *dcc)
{
    memset(dcc, 0, sizeof(FFStatistic));
}
