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
#ifndef COMPAREVIEWER_H
#define COMPAREVIEWER_H

#include <QWidget>
#include <QSplitter>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include "ImgViewer.h"

class CompareViewer : public QWidget {
    Q_OBJECT

public:
    explicit CompareViewer(QWidget *parentWindow, QWidget *parent = nullptr);
    ~CompareViewer();

protected:
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openFileA();
    void openFileB();
    void openFolderA();
    void openFolderB();
    void onLoadA();
    void onLoadB();
    void onSyncZoomPanToggled(bool checked);
    void onFrameSyncToggled(bool checked);
    void onPreviousFrame();
    void onNextFrame();
    void onRotateLeft();
    void onRotateRight();
    void onFitWindow();
    void onPixelInfoA(int x, int y, int r, int g, int b);
    void onPixelInfoB(int x, int y, int r, int g, int b);

private:
    struct PanelWidgets {
        QComboBox *formatCombo;
        QLineEdit *widthEdit;
        QLineEdit *heightEdit;
        QLineEdit *startFrameEdit;
        QLineEdit *endFrameEdit;
        QPushButton *openFileBtn;
        QPushButton *openFolderBtn;
        QPushButton *loadBtn;
        QLabel *filenameLabel;
    };

    QWidget* createPanel(PanelWidgets &pw);
    void loadFilesToPanel(int panelIndex, QStringList filelist, bool autoParse = true);
    void parseFilename(const QString &filepath, QString &format, int &W, int &H);
    void matchFiles();

    QWidget *parentWindow;
    QSplitter *splitter;
    ImgViewer *imgViewerA;
    ImgViewer *imgViewerB;

    PanelWidgets pwA;
    PanelWidgets pwB;

    // 已加载的文件列表（用于参数修改后重载）
    QStringList loadedFilesA;
    QStringList loadedFilesB;

    // 工具栏
    QPushButton *rotateLeftBtn;
    QPushButton *rotateRightBtn;
    QPushButton *fitBtn;

    // 同步控制
    QCheckBox *syncZoomPanCheck;
    QCheckBox *frameSyncCheck;
    QPushButton *prevFrameBtn;
    QPushButton *nextFrameBtn;
    QLabel *pixelInfoLabel;

    bool syncing;
    QStringList formatNames;
};

#endif // COMPAREVIEWER_H
