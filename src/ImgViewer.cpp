/*
 * This file is part of the https://github.com/QQxiaoming/YUVviewer.git
 * project.
 *
 * Copyright (C) 2020 Quard <2014500726@smail.xtu.edu.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include <QImage>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QPoint>
#include <QPainter>
#include <QMessageBox>
#include <QDebug>
#include "filedialog.h"
#include "ImgViewer.h"
#include "YUVdecoder.h"
#include "ui_UI_ImgViewer.h"

YUVDecodeThread::YUVDecodeThread(QWidget *parent,const QString &yuvfilename,QString YUVFormat,
                                 int W, int H, int startframe, int totalframe) :
    QThread(parent) {
    this->window = parent;
    this->yuvfilename = yuvfilename;
    this->W = W;
    this->H = H;
    this->startframe = startframe;
    this->totalframe = totalframe;
    // 获取该格式的解码函数
    this->decoder = ImageDecoder::yuvdecoder_map.find(YUVFormat).value();
}

void YUVDecodeThread::run() {
    // 定义img列表用了保存每一帧的QImage*
    QList<QImage*> img_RGB_list;
    if(this->decoder == nullptr) {
        // 未能成功获取则返回无法解码
        emit finsh_signal(img_RGB_list,nullptr);
    } else {
        QList<cv::Mat*> frame_RGB_list;
        try{
            // 成功获取则返回计算结果
            frame_RGB_list = this->decoder(this->yuvfilename, this->W, this->H, this->startframe, this->totalframe);
        } catch(cv::Exception& e) {
            emit finsh_signal(img_RGB_list,nullptr);
            return;
        }
        // 将原始帧转换到QImage*并保存到img列表
        foreach( cv::Mat* img,frame_RGB_list) {
            // 提取图像的通道和尺寸，用于将OpenCV下的image转换成Qimage
            QImage *qImg = new QImage((const unsigned char*)(img->data), img->cols, img->rows, img->step, QImage::Format_RGB888,(QImageCleanupFunction)this->image_cleanup,img);
            *qImg = qImg->rgbSwapped();
            img_RGB_list.insert(img_RGB_list.end(), qImg);
        }
        emit finsh_signal(img_RGB_list,this->yuvfilename);
    }
}

void YUVDecodeThread::image_cleanup(cv::Mat* ptr) {
    delete ptr;
}

ImgViewer::ImgViewer(const QString &folderpath, QWidget *parent,QWidget *parentWindow, int frameRate) :
    QWidget(parent),
    ui(new Ui::ImgViewerWindow) {
    ui->setupUi(this);
    qRegisterMetaType<QList<QImage*>>("QList<QImage*>");
    this->parentWindow = parentWindow;
    this->folderpath = folderpath;
    this->frameRate = frameRate;
    imgExportWindow = new ImgExport(this);
    setWindowTitle("loading file, please wait ....");
    ui->left_PushButton->setFlat(true);
    ui->right_PushButton->setFlat(true);
    ui->left_PushButton->setFocusPolicy(Qt::NoFocus);
    ui->right_PushButton->setFocusPolicy(Qt::NoFocus);
    ui->play_PushButton->setFocusPolicy(Qt::NoFocus);
    ui->progress_Slider->setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    QObject::connect(ui->left_PushButton, SIGNAL(clicked()), this, SLOT(previousImg()));
    QObject::connect(ui->right_PushButton, SIGNAL(clicked()), this, SLOT(nextImg()));
    QObject::connect(ui->play_PushButton, SIGNAL(clicked()), this, SLOT(togglePlay()));
    QObject::connect(ui->progress_Slider, SIGNAL(valueChanged(int)), this, SLOT(onSliderChanged(int)));

    playTimer = new QTimer(this);
    QObject::connect(playTimer, SIGNAL(timeout()), this, SLOT(onPlayTimer()));

    ui->playbackBar->setStyleSheet("QWidget#playbackBar { background-color: rgba(0, 0, 0, 128); }"
                                   "QLabel { color: white; }"
                                   "QPushButton { color: white; }");
    ui->playbackBar->raise();

    left_click = false;
}

ImgViewer::~ImgViewer() {
    delete imgExportWindow;
    delete ui;
}

bool ImgViewer::setFileList(QStringList filenamelist,QString YUVFormat, int W, int H, int startframe, int totalframe) {
    // 获取该格式的解码函数
    ImageDecoder::yuvdecoder_t decoder = ImageDecoder::yuvdecoder_map.find(YUVFormat).value();
    if(decoder == nullptr) {
        // 未能成功获取则返回无法解码
        return false;
    } else {
        // 成功获取解码器则准备解码
        ui->imgViewer->setText("");
        // 遍历文件列表
        foreach( QString filename, filenamelist) {
            QList<cv::Mat*> frame_RGB_list;
            try{
                // 使用获取的解码函数进行解码得到RGB的原始帧列表
                frame_RGB_list = decoder(filename, W, H, startframe, totalframe);
            } catch(cv::Exception& e) {
                continue;
            }
            if (frame_RGB_list.empty()) {
                return false;
            }
            // 定义img列表用来保存每一帧的Qimage*
            QList<QImage*> img_RGB_list;
            // 将原始帧转换到Qimage*并保存到img列表
            foreach (cv::Mat* img, frame_RGB_list) {
                // 提取图像的通道和尺寸，用于将OpenCV下的image转换成Qimage
                QImage *qImg = new QImage((const unsigned char*)(img->data), img->cols, img->rows, img->step, QImage::Format_RGB888, (QImageCleanupFunction)this->image_cleanup,img);
                *qImg = qImg->rgbSwapped();
                img_RGB_list.insert(img_RGB_list.end(), qImg);
            }
            // img_RGB_list以及文件名存入列表
            this->img_list.insert(this->img_list.end(),img_RGB_list);
            QFileInfo fileInfo(filename);
            this->filelist.insert(this->filelist.end(),fileInfo.fileName());
        }
        // 设置显示第一个YUV文件的第一帧图像
        this->currentImg_RGB_list = this->img_list.at(0);
        this->currentImg = this->currentImg_RGB_list.at(0);
        this->setWindowTitle(this->filelist.at(0)+"-0");
        this->rotation = 0;
        this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
        applyRotation();
        this->point = QPoint(0, 0);
        updateSlider();
        updateFrameInfo();
        emit currentFileChanged(this->filelist.at(0), 0);
        return true;
    }
}

void ImgViewer::image_cleanup(cv::Mat* ptr) {
    delete ptr;
}

void ImgViewer::reciveimgdata(QList<QImage*> img_RGB_list,QString filename) {
    if (!img_RGB_list.empty()) {
        // img_RGB_list以及文件名存入列表
        this->img_list.insert(this->img_list.end(),img_RGB_list);
        QFileInfo fileInfo(filename);
        this->filelist.insert(this->filelist.end(),fileInfo.fileName());
        if(this->img_list.count() == 1) {
            // 设置显示第一个YUV文件的第一帧图像
            ui->imgViewer->setText("");
            this->currentImg_RGB_list = this->img_list.at(0);
            this->currentImg = this->currentImg_RGB_list.at(0);
            this->setWindowTitle(this->filelist.at(0)+"-0");
            this->rotation = 0;
            this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
            applyRotation();
            this->point = QPoint(0, 0);
            this->repaint();
            emit currentFileChanged(this->filelist.at(0), 0);
        }
        updateSlider();
        updateFrameInfo();
    }

    this->decode_thread.pop_front();
    if (!this->decode_thread.empty()) {
        this->decode_thread[0]->start();
    } else {
        if(this->img_list.empty()) {
            QMessageBox::critical(this, "Error", "unknow error!!", QMessageBox::Ok);
            this->close();
        }
    }
}

bool ImgViewer::setFileList_multithreading(QStringList filenamelist,QString YUVFormat,
                                            int W, int H, int startframe, int totalframe) {
    // 获取该格式的解码函数
    ImageDecoder::yuvdecoder_t decoder = ImageDecoder::yuvdecoder_map.find(YUVFormat).value();
    if(decoder == nullptr) {
        // 未能成功获取则返回无法解码
        return false;
    }
    // 遍历文件列表
    foreach( QString filename, filenamelist) {
        YUVDecodeThread *decodeThread = new YUVDecodeThread(this, filename, YUVFormat, W, H, startframe, totalframe);
        QObject::connect(decodeThread, SIGNAL(finsh_signal(QList<QImage*>, QString)), this, SLOT(reciveimgdata(QList<QImage*>, QString)));
        this->decode_thread.insert(this->decode_thread.end(),decodeThread);
    }
    this->decode_thread[0]->start();
    return true;
}

void ImgViewer::syncFrom(QPoint newPoint, QImage newScaledImg) {
    this->point = newPoint;
    this->scaled_img = newScaledImg;
    this->repaint();
}

void ImgViewer::syncPosition(ImgViewer *other) {
    if (!other || this->img_list.empty() || other->img_list.empty()) return;
    // 将当前面板的显示位置同步到 other 面板
    int src_file_idx = other->img_list.indexOf(other->currentImg_RGB_list);
    int src_frame_idx = other->currentImg_RGB_list.indexOf(other->currentImg);
    // 限制在当前面板的有效范围内
    int dst_file_idx = qMin(src_file_idx, this->img_list.count() - 1);
    int dst_frame_idx = qMin(src_frame_idx, this->img_list[dst_file_idx].count() - 1);
    this->currentImg_RGB_list = this->img_list[dst_file_idx];
    this->currentImg = this->currentImg_RGB_list[dst_frame_idx];
    this->point = QPoint(0, 0);
    this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
    applyRotation();
    this->repaint();
}

QString ImgViewer::getCurrentFileName() const {
    if (this->img_list.empty() || !this->currentImg) return QString();
    int list_index = this->img_list.indexOf(this->currentImg_RGB_list);
    if (list_index < 0 || list_index >= this->filelist.size()) return QString();
    return this->filelist[list_index];
}

int ImgViewer::getCurrentFrameIndex() const {
    if (!this->currentImg_RGB_list.isEmpty() && this->currentImg) {
        return this->currentImg_RGB_list.indexOf(this->currentImg);
    }
    return 0;
}

void ImgViewer::rotateLeft() {
    if (this->img_list.empty()) return;
    this->rotation = (this->rotation + 270) % 360;
    fitToWindow();
    emit viewChanged(this->point, this->scaled_img);
}

void ImgViewer::rotateRight() {
    if (this->img_list.empty()) return;
    this->rotation = (this->rotation + 90) % 360;
    fitToWindow();
    emit viewChanged(this->point, this->scaled_img);
}

void ImgViewer::applyRotation() {
    if (this->rotation != 0) {
        QTransform rot;
        rot.rotate(this->rotation);
        this->scaled_img = this->scaled_img.transformed(rot);
    }
    if (this->flipRGB) {
        this->scaled_img = this->scaled_img.rgbSwapped();
    }
}

void ImgViewer::fitToWindow() {
    if (this->img_list.empty() || !this->currentImg) return;
    this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
    applyRotation();
    this->point = QPoint(0, 0);
    this->repaint();
}

void ImgViewer::closeEvent(QCloseEvent *event) {
    if (playTimer->isActive()) {
        playTimer->stop();
    }
    this->parentWindow->show();
    event->accept();
    if(!this->img_list.empty()) {
        foreach(QList<QImage*> list,this->img_list) {
            if(!list.empty()) {
                foreach(QImage* img,list) {
                    delete img;
                }
            }
        }
    }
}

void ImgViewer::draw_img(QPainter *painter) {
    painter->drawPixmap(this->point, QPixmap::fromImage(this->scaled_img));
}

void ImgViewer::draw_info(QPainter *painter) {
    if (isMouseInImg) {
        int r = qRed(currentMousePosColor);
        int g = qGreen(currentMousePosColor);
        int b = qBlue(currentMousePosColor);
        int x = currentMousePos.x();
        int y = currentMousePos.y();
        QString info = QString("x:%1 y:%2 R:%3 G:%4 B:%5").arg(x).arg(y).arg(r).arg(g).arg(b);
        QPen pen;
        pen.setColor(Qt::black);
        painter->setPen(pen);
        painter->drawText(20, this->height() - 20, info);
    }
}

void ImgViewer::paintEvent(QPaintEvent *event) {
    if (!this->img_list.empty()) {
        QPainter painter;
        painter.begin(this);
        draw_img(&painter);
        draw_info(&painter);
        painter.end();
    }
    Q_UNUSED(event);
}

void ImgViewer::mouseMoveEvent(QMouseEvent *event) {
    if (!this->img_list.empty()) {
        QRgb current_color = currentMousePosColor;
        bool current_isMouseInImg = isMouseInImg;
        if (this->scaled_img.rect().contains(event->pos()-this->point)) {
            isMouseInImg = true;
            currentMousePosColor = this->scaled_img.pixel(event->pos()-this->point);
            currentMousePos = event->pos()-this->point;
        } else {
            isMouseInImg = false;
        }
        if( this->left_click) {
            this->endPos = event->pos() - this->startPos;
            this->point = this->point + this->endPos;
            this->startPos = event->pos();
            this->update();
            emit viewChanged(this->point, this->scaled_img);
        } else {
            if(current_color != currentMousePosColor || current_isMouseInImg != isMouseInImg) {
                this->update();
                if(isMouseInImg) {
                    int r = qRed(currentMousePosColor);
                    int g = qGreen(currentMousePosColor);
                    int b = qBlue(currentMousePosColor);
                    emit pixelInfoChanged(currentMousePos.x(), currentMousePos.y(), r, g, b);
                }
            }
        }
    }
    (void)event;
}

void ImgViewer::mousePressEvent(QMouseEvent *event) {
    if (!this->img_list.empty()) {
        if( event->button() == Qt::LeftButton) {
            this->left_click = true;
            this->startPos = event->pos();
        }
    }
}

void ImgViewer::mouseReleaseEvent(QMouseEvent *event) {
    if (!this->img_list.empty()) {
        if( event->button() == Qt::LeftButton) {
            this->left_click = false;
        } else if(event->button() == Qt::RightButton) {
            this->point = QPoint(0, 0);
            this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
            applyRotation();
            this->repaint();
            emit viewChanged(this->point, this->scaled_img);
        } else if(event->button() == Qt::MiddleButton) {
            this->scaled_img = this->currentImg->scaled(this->currentImg->size().width(),this->currentImg->size().height());
            applyRotation();
            this->point = QPoint(0, 0);
            this->repaint();
            emit viewChanged(this->point, this->scaled_img);
        }
    }
}

void ImgViewer::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!this->img_list.empty()) {
        if( event->button() == Qt::LeftButton) {
            int list_index = img_list.indexOf(currentImg_RGB_list);
            QList<QImage*> img_RGB_list = img_list[list_index];
            int img_index = img_RGB_list.indexOf(currentImg);
            QString name = folderpath + "/" + filelist[list_index].replace(".yuv","-").replace(".data","-").replace(".raw","-").replace(".png","-") + QString::number(img_index);
            imgExportWindow->setSaveFileName(name);
            imgExportWindow->setSaveImage(currentImg);
            imgExportWindow->show();
        } else if(event->button() == Qt::RightButton) {
            this->flipRGB = this->flipRGB ? false : true;
            this->point = QPoint(0, 0);
            this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
            applyRotation();
            this->repaint();
        }
    }
}

void ImgViewer::wheelEvent(QWheelEvent *event) {
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    double event_x=event->position().x();
    double event_y=event->position().y();
    #else
    double event_x=event->x();
    double event_y=event->y();
    #endif
    if (!this->img_list.empty()) {
        if( event->angleDelta().y() > 0) {
            // 放大图片
            if( this->scaled_img.width() != 0 && this->scaled_img.height() != 0) {
                float setpsize_x = ((float)this->scaled_img.width())/16.0f;
                float setpsize_y = ((float)this->scaled_img.height())/16.0f; //缩放可能导致比例不精确

                this->scaled_img = this->currentImg->scaled(
                    this->scaled_img.width() + setpsize_x,this->scaled_img.height() + setpsize_y);
                applyRotation();
                float new_w = event_x -
                    (this->scaled_img.width() * (event_x - this->point.x())) / (this->scaled_img.width() - setpsize_x);
                float new_h = event_y -
                    (this->scaled_img.height() * (event_y - this->point.y())) / (this->scaled_img.height() - setpsize_y);
                this->point = QPoint(new_w, new_h);
                this->repaint();
            }
        } else if( event->angleDelta().y() < 0) {
            // 缩小图片
            if(this->scaled_img.width() > 25 && this->scaled_img.height() > 25)
            {
                float setpsize_x = ((float)this->scaled_img.width())/16.0f;
                float setpsize_y = ((float)this->scaled_img.height())/16.0f; //缩放可能导致比例不精确

                this->scaled_img = this->currentImg->scaled(
                    this->scaled_img.width() - setpsize_x,this->scaled_img.height() - setpsize_y);
                applyRotation();
                float new_w = event_x -
                    (this->scaled_img.width() * (event_x - this->point.x())) / (this->scaled_img.width() + setpsize_x);
                float new_h = event_y -
                    (this->scaled_img.height() * (event_y - this->point.y())) / (this->scaled_img.height() + setpsize_y);
                this->point = QPoint(new_w, new_h);
                this->repaint();
            }
        }
        emit viewChanged(this->point, this->scaled_img);
    }
}

void ImgViewer::resizeEvent(QResizeEvent *event) {
    if (!this->img_list.empty()) {
        this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
        applyRotation();
        this->point = QPoint(0, 0);
        this->update();
    }
    int barH = ui->playbackBar->sizeHint().height();
    ui->playbackBar->setGeometry(0, this->height() - barH, this->width(), barH);
    (void)event;
}

void ImgViewer::previousImg() {
    if (!this->img_list.empty()) {
        //得到当前显示的文件序号
        int list_index = this->img_list.indexOf(this->currentImg_RGB_list);
        QList<QImage*> img_RGB_list = this->img_list[list_index];
        //得到当前显示的图像是文件的帧序号
        int img_index = img_RGB_list.indexOf(this->currentImg);

        //判断当前是否是第一帧
        if(img_index == 0) {
            //如果当前是第一帧,则判断当前是否是第一个文件
            if(list_index == 0) {
                //如果是第一个文件则文件序号更新代到最后一个序号
                list_index = this->img_list.count() - 1;
            } else {
                //否则文件序号更新到前一个文件序号
                list_index -= 1;
            }
            //更新帧序号为文件的最后一帧序号
            img_index = this->img_list[list_index].count() - 1;
        } else {
            //否则更新帧序号为前一帧序号,此时文件序号不用更新
            img_index -= 1;
        }

        //序号更新完成,代入序号配置当前显示的页面
        setWindowTitle(this->filelist[list_index] + "-" + QString::number(img_index));
        this->currentImg_RGB_list = this->img_list[list_index];
        this->currentImg = this->currentImg_RGB_list[img_index];
        this->point = QPoint(0, 0);
        this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
        applyRotation();
        this->repaint();
        updateSlider();
        updateFrameInfo();
        emit currentFileChanged(this->filelist[list_index], img_index);
    }
}

void ImgViewer::nextImg() {
    if (!this->img_list.empty()) {
        //得到当前显示的文件序号
        int list_index = this->img_list.indexOf(this->currentImg_RGB_list);
        QList<QImage*> img_RGB_list = this->img_list[list_index];
        //得到当前显示的图像是文件的帧序号
        int img_index = img_RGB_list.indexOf(this->currentImg);

        //判断当前是否是最后一帧
        if(img_index == img_RGB_list.count() - 1) {
            //如果当前是最后一帧,则判断当前是否是最后一个文件
            if(list_index == this->img_list.count() - 1) {
                //如果是最后一个文件则文件序号更新代到第一个序号
                list_index = 0;
            } else {
                //否则文件序号更新到后一个文件序号
                list_index += 1;
            }
            //更新帧序号为文件的第一帧序号
            img_index = 0;
        } else {
            //否则更新帧序号为后一帧序号,此时文件序号不用更新
            img_index += 1;
        }

        //序号更新完成,代入序号配置当前显示的页面
        setWindowTitle(this->filelist[list_index] + "-" + QString::number(img_index));
        this->currentImg_RGB_list = this->img_list[list_index];
        this->currentImg = this->currentImg_RGB_list[img_index];
        this->point = QPoint(0, 0);
        this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
        applyRotation();
        this->repaint();
        updateSlider();
        updateFrameInfo();
        emit currentFileChanged(this->filelist[list_index], img_index);
    }
}

int ImgViewer::getTotalFrameCount() const {
    int total = 0;
    for (const auto &list : this->img_list) {
        total += list.count();
    }
    return total;
}

int ImgViewer::getCurrentGlobalIndex() const {
    if (this->img_list.empty()) return 0;
    int globalIndex = 0;
    for (int i = 0; i < this->img_list.count(); i++) {
        if (this->img_list[i] == this->currentImg_RGB_list) {
            globalIndex += this->currentImg_RGB_list.indexOf(this->currentImg);
            break;
        }
        globalIndex += this->img_list[i].count();
    }
    return globalIndex;
}

void ImgViewer::goToGlobalIndex(int globalIndex) {
    if (this->img_list.empty()) return;
    int total = getTotalFrameCount();
    if (total == 0) return;
    globalIndex = qBound(0, globalIndex, total - 1);

    int accumulated = 0;
    for (int i = 0; i < this->img_list.count(); i++) {
        int count = this->img_list[i].count();
        if (accumulated + count > globalIndex) {
            int frameIdx = globalIndex - accumulated;
            this->currentImg_RGB_list = this->img_list[i];
            this->currentImg = this->currentImg_RGB_list[frameIdx];
            setWindowTitle(this->filelist[i] + "-" + QString::number(frameIdx));
            this->point = QPoint(0, 0);
            this->scaled_img = this->currentImg->scaled(this->size(), Qt::KeepAspectRatio);
            applyRotation();
            this->repaint();
            emit currentFileChanged(this->filelist[i], frameIdx);
            return;
        }
        accumulated += count;
    }
}

void ImgViewer::togglePlay() {
    if (this->img_list.empty()) return;
    if (isPlaying) {
        playTimer->stop();
        isPlaying = false;
    } else {
        playTimer->start(1000 / frameRate);
        isPlaying = true;
    }
    updatePlayButton();
}

void ImgViewer::onPlayTimer() {
    if (this->img_list.empty()) return;
    int list_index = this->img_list.indexOf(this->currentImg_RGB_list);
    QList<QImage*> imgList = this->img_list[list_index];
    int img_index = imgList.indexOf(this->currentImg);

    bool atEnd = (list_index == this->img_list.count() - 1) &&
                 (img_index == imgList.count() - 1);
    if (atEnd) {
        playTimer->stop();
        isPlaying = false;
        updatePlayButton();
        return;
    }
    nextImg();
}

void ImgViewer::updatePlayButton() {
    if (isPlaying) {
        ui->play_PushButton->setText("⏸");
    } else {
        ui->play_PushButton->setText("▶");
    }
}

void ImgViewer::updateSlider() {
    int total = getTotalFrameCount();
    if (total <= 1) return;
    ui->progress_Slider->blockSignals(true);
    ui->progress_Slider->setMaximum(total - 1);
    ui->progress_Slider->setValue(getCurrentGlobalIndex());
    ui->progress_Slider->blockSignals(false);
}

void ImgViewer::updateFrameInfo() {
    int total = getTotalFrameCount();
    int current = getCurrentGlobalIndex();
    ui->frameInfo_Label->setText(QString("%1/%2").arg(current + 1).arg(total));
}

void ImgViewer::onSliderChanged(int value) {
    if (this->img_list.empty()) return;
    goToGlobalIndex(value);
    updateFrameInfo();
}

void ImgViewer::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Space:
        togglePlay();
        break;
    case Qt::Key_Left:
        if (isPlaying) togglePlay();
        previousImg();
        break;
    case Qt::Key_Right:
        if (isPlaying) togglePlay();
        nextImg();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}
