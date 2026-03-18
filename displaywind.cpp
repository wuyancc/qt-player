#include "displaywind.h"
#include "ui_displaywind.h"
#include <QDebug>
#include <QPainter>
DisplayWind::DisplayWind(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DisplayWind)
{
    ui->setupUi(this);
    win_width_ = width();
    win_height_ = height();
    memset(&dst_video_frame_, sizeof(VideoFrame), 0);
    play_state_ = 2;
}

DisplayWind::~DisplayWind()
{
    QMutexLocker locker(&m_mutex);
    delete ui;
    DeInit();
}

int DisplayWind::Draw(const Frame *frame)
{
    QMutexLocker locker(&m_mutex);
    if(!img_scaler_ || req_resize_) {
        if(img_scaler_) {
            DeInit();
        }
        win_width_ = width();
        win_height_ = height();
        video_width = frame->width;
        video_height = frame->height;
        img_scaler_ = new ImageScaler();
        double video_aspect_ratio = frame->width * 1.0 / frame->height;
        double win_aspect_ratio = win_width_ * 1.0 / win_height_;
        if(win_aspect_ratio > video_aspect_ratio) {
            //此时应该是调整x的起始位置，以高度为基准
            img_height = win_height_;
            img_height &= 0xfffc;
            img_width = img_height * video_aspect_ratio;
            img_width &= 0xfffc;
            y_ = 0;
            x_ = (win_width_ - img_width) / 2;
        } else {
            //此时应该是调整y的起始位置，以宽度为基准
            img_width = win_width_;
            img_width &= 0xfffc;
            img_height = img_width / video_aspect_ratio;
            img_height &= 0xfffc;
            x_ = 0;
            y_ = (win_height_ - img_height) / 2;
        }
        img_scaler_->Init(video_width, video_height, frame->format,
                          img_width, img_height, AV_PIX_FMT_RGB24);
        memset(&dst_video_frame_, 0, sizeof(VideoFrame));
        dst_video_frame_.width = img_width;
        dst_video_frame_.height = img_height;
        dst_video_frame_.format = AV_PIX_FMT_RGB24;
        dst_video_frame_.data[0] = (uint8_t*)malloc(img_width * img_height * 3);
        dst_video_frame_.linesize[0] = img_width * 3; // 每行的字节数
        req_resize_ = false;
    }
    img_scaler_->Scale3(frame, &dst_video_frame_);
    QImage imageTmp =  QImage((uint8_t *)dst_video_frame_.data[0],
                              img_width, img_height, QImage::Format_RGB888);
    // img = imageTmp.copy(0, 0, img_width, img_height);
    img = imageTmp;
    update();
    //    repaint();
    return 0;
}

void DisplayWind::DeInit()
{
    if(dst_video_frame_.data[0]) {
        free(dst_video_frame_.data[0]);
        dst_video_frame_.data[0] = NULL;
    }
    if(img_scaler_) {
        delete img_scaler_;
        img_scaler_ = NULL;
    }
}

void DisplayWind::StartPlay()
{
    QMutexLocker locker(&m_mutex);
    play_state_ = 1;
}

void DisplayWind::StopPlay()
{
    QMutexLocker locker(&m_mutex);
    play_state_ = 2;
    update();
}


void DisplayWind::paintEvent(QPaintEvent *)
{
    QMutexLocker locker(&m_mutex);
    if(play_state_ == 1) {  // 播放状态
        if (img.isNull()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        //    //    p.translate(X, Y);
        //    //    p.drawImage(QRect(0, 0, W, H), img);
        QRect rect = QRect(x_, y_, img.width(), img.height());
        //        qDebug() << rect << ", win_w:" << this->width() << ", h:" << this->height();
        painter.drawImage(rect, img.scaled(img.width(), img.height()));
    } else if(play_state_ == 2) {
        QPainter p(this);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawRect(rect());
    }
}


//void DisplayWind::paintEvent(QPaintEvent *)
//{
//    QMutexLocker locker(&m_mutex);
//    if(play_state_ == 1) {  // 播放状态
//        if (img.isNull()) {
//            return;
//        }
//        QPainter painter(this);
//        //        painter.setRenderHint(QPainter::Antialiasing, true);
//        painter.setRenderHint(QPainter::HighQualityAntialiasing);
//        int w = this->width();
//        int h = this->height();
//        img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
//        //    //    p.translate(X, Y);
//        //    //    p.drawImage(QRect(0, 0, W, H), img);
//        QRect rect = QRect(x_, y_, w, h);
//        painter.drawImage(rect, img);
//    } else if(play_state_ == 2) {
//        QPainter p(this);
//        p.setPen(Qt::NoPen);
//        p.setBrush(Qt::black);
//        p.drawRect(rect());
//    }
//}


void DisplayWind::resizeEvent(QResizeEvent *event)
{
    QMutexLocker locker(&m_mutex);
    if(win_width_ != width() || win_height_ != height()) {
        //        DeInit();       // 释放尺寸缩放资源，等下一次draw的时候重新初始化
        //        win_width = width();
        //        win_height = height();
        req_resize_ = true;
    }
}
