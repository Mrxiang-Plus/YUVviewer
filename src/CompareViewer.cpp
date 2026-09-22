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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "CompareViewer.h"
#include "filedialog.h"
#include "YUVdecoder.h"

CompareViewer::CompareViewer(QWidget *parentWindow, QWidget *parent) :
    QWidget(parent),
    parentWindow(parentWindow),
    syncing(false) {
    setWindowTitle(tr("Image Compare"));
    setMinimumSize(1024, 600);
    setAcceptDrops(true);

    formatNames = ImageDecoder::yuvdecoder_map.keys();

    // ===== 顶部工具栏 =====
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    syncZoomPanCheck = new QCheckBox(tr("Sync Zoom/Pan"));
    syncZoomPanCheck->setChecked(true);
    frameSyncCheck = new QCheckBox(tr("Sync Frame"));
    frameSyncCheck->setChecked(true);
    rotateLeftBtn = new QPushButton(tr("Rotate Left"));
    rotateRightBtn = new QPushButton(tr("Rotate Right"));
    fitBtn = new QPushButton(tr("Fit Window"));
    prevFrameBtn = new QPushButton(tr("< Previous Frame"));
    nextFrameBtn = new QPushButton(tr("Next Frame >"));
    syncPlayBtn = new QPushButton(tr("▶ Play"));
    toolbarLayout->addWidget(syncZoomPanCheck);
    toolbarLayout->addWidget(frameSyncCheck);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(rotateLeftBtn);
    toolbarLayout->addWidget(rotateRightBtn);
    toolbarLayout->addWidget(fitBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(syncPlayBtn);
    toolbarLayout->addWidget(prevFrameBtn);
    toolbarLayout->addWidget(nextFrameBtn);

    pixelInfoLabel = new QLabel;

    imgViewerA = nullptr;
    imgViewerB = nullptr;

    QWidget *panelA = createPanel(pwA);
    QWidget *panelB = createPanel(pwB);

    panelA->setMinimumWidth(300);
    panelB->setMinimumWidth(300);
    panelA->setProperty("panelIndex", 0);
    panelB->setProperty("panelIndex", 1);

    splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(panelA);
    splitter->addWidget(panelB);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(pixelInfoLabel);

    // ===== 信号连接 =====
    connect(pwA.openFileBtn, &QPushButton::clicked, this, &CompareViewer::openFileA);
    connect(pwB.openFileBtn, &QPushButton::clicked, this, &CompareViewer::openFileB);
    connect(pwA.openFolderBtn, &QPushButton::clicked, this, &CompareViewer::openFolderA);
    connect(pwB.openFolderBtn, &QPushButton::clicked, this, &CompareViewer::openFolderB);
    connect(pwA.loadBtn, &QPushButton::clicked, this, &CompareViewer::onLoadA);
    connect(pwB.loadBtn, &QPushButton::clicked, this, &CompareViewer::onLoadB);
    connect(syncZoomPanCheck, &QCheckBox::toggled, this, &CompareViewer::onSyncZoomPanToggled);
    connect(frameSyncCheck, &QCheckBox::toggled, this, &CompareViewer::onFrameSyncToggled);
    connect(prevFrameBtn, &QPushButton::clicked, this, &CompareViewer::onPreviousFrame);
    connect(nextFrameBtn, &QPushButton::clicked, this, &CompareViewer::onNextFrame);
    connect(rotateLeftBtn, &QPushButton::clicked, this, &CompareViewer::onRotateLeft);
    connect(rotateRightBtn, &QPushButton::clicked, this, &CompareViewer::onRotateRight);
    connect(fitBtn, &QPushButton::clicked, this, &CompareViewer::onFitWindow);
    connect(syncPlayBtn, &QPushButton::clicked, this, &CompareViewer::onSyncPlay);

    syncPlayTimer = new QTimer(this);
    connect(syncPlayTimer, &QTimer::timeout, this, &CompareViewer::onSyncPlayTimer);

    loadDumpFormats();
}

CompareViewer::~CompareViewer() {
}

QWidget* CompareViewer::createPanel(PanelWidgets &pw) {
    QWidget *panel = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *paramGroup = new QGroupBox(tr("Parameters"));
    QGridLayout *paramLayout = new QGridLayout(paramGroup);

    paramLayout->addWidget(new QLabel(tr("Format:")), 0, 0);
    pw.formatCombo = new QComboBox;
    pw.formatCombo->setStyleSheet("combobox-popup: 0;");
    pw.formatCombo->addItems(formatNames);
    pw.formatCombo->setCurrentText("YV12");
    paramLayout->addWidget(pw.formatCombo, 0, 1, 1, 2);

    paramLayout->addWidget(new QLabel(tr("Width:")), 1, 0);
    pw.widthEdit = new QLineEdit("1920");
    pw.widthEdit->setValidator(new QIntValidator(1, 65536, this));
    paramLayout->addWidget(pw.widthEdit, 1, 1);
    paramLayout->addWidget(new QLabel(tr("Height:")), 1, 2);
    pw.heightEdit = new QLineEdit("1080");
    pw.heightEdit->setValidator(new QIntValidator(1, 65536, this));
    paramLayout->addWidget(pw.heightEdit, 1, 3);

    paramLayout->addWidget(new QLabel(tr("Start:")), 2, 0);
    pw.startFrameEdit = new QLineEdit("0");
    pw.startFrameEdit->setValidator(new QIntValidator(0, 999999, this));
    paramLayout->addWidget(pw.startFrameEdit, 2, 1);
    paramLayout->addWidget(new QLabel(tr("End:")), 2, 2);
    pw.endFrameEdit = new QLineEdit("0");
    pw.endFrameEdit->setValidator(new QIntValidator(0, 999999, this));
    paramLayout->addWidget(pw.endFrameEdit, 2, 3);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    pw.openFileBtn = new QPushButton(tr("Open File"));
    pw.openFolderBtn = new QPushButton(tr("Open Folder"));
    pw.loadBtn = new QPushButton(tr("Load"));
    pw.loadBtn->setEnabled(false);
    btnLayout->addWidget(pw.openFileBtn);
    btnLayout->addWidget(pw.openFolderBtn);
    btnLayout->addWidget(pw.loadBtn);
    paramLayout->addLayout(btnLayout, 4, 0, 1, 4);

    layout->addWidget(paramGroup);

    pw.filenameLabel = new QLabel;
    pw.filenameLabel->setStyleSheet("color: #333; font-weight: bold; padding: 2px;");
    pw.filenameLabel->setAlignment(Qt::AlignCenter);
    pw.filenameLabel->hide();
    layout->addWidget(pw.filenameLabel);

    QLabel *placeholder = new QLabel(tr("Click 'Open File' or drag files here"));
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: gray; font-size: 14px;");
    placeholder->setObjectName("placeholder");
    layout->addWidget(placeholder, 1);

    return panel;
}

// ===== 配置加载 =====
void CompareViewer::loadDumpFormats() {
    QString configPath = QDir::homePath() + "/.YUVViewer/dump_formats.json";
    QFile configFile(configPath);
    if (!configFile.open(QFile::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    configFile.close();
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();
    QJsonObject codes = root["format_codes"].toObject();
    for (auto it = codes.begin(); it != codes.end(); ++it)
        formatCodeMap[it.key()] = it.value().toString();
    QJsonObject exts = root["extension_defaults"].toObject();
    for (auto it = exts.begin(); it != exts.end(); ++it)
        extensionDefaultMap[it.key()] = it.value().toString();
    QJsonArray patterns = root["patterns"].toArray();
    for (const QJsonValue &v : patterns) {
        QJsonObject obj = v.toObject();
        DumpFormatPattern p;
        p.name = obj["name"].toString();
        p.regex = QRegularExpression(obj["regex"].toString());
        p.widthGroup = obj["width_group"].toInt(0);
        p.heightGroup = obj["height_group"].toInt(0);
        p.formatGroup = obj["format_group"].toInt(0);
        if (p.regex.isValid())
            dumpPatterns.append(p);
    }

    QJsonArray rules = root["sync_rules"].toArray();
    for (const QJsonValue &v : rules) {
        QJsonObject obj = v.toObject();
        SyncRule r;
        r.name = obj["name"].toString();
        r.keyRegex = QRegularExpression(obj["key_regex"].toString());
        r.keyGroup = obj["key_group"].toInt(1);
        if (r.keyRegex.isValid())
            syncRules.append(r);
    }
}

// ===== 参数自动解析 =====
void CompareViewer::parseFilename(const QString &filepath, QString &format, int &W, int &H) {
    QString baseName = QFileInfo(filepath).fileName();
    QString ext = QFileInfo(filepath).suffix().toLower();
    W = 0;
    H = 0;
    format.clear();

    for (const DumpFormatPattern &p : dumpPatterns) {
        QRegularExpressionMatch m = p.regex.match(baseName);
        if (!m.hasMatch()) continue;
        if (p.widthGroup > 0) W = m.captured(p.widthGroup).toInt();
        if (p.heightGroup > 0) H = m.captured(p.heightGroup).toInt();
        if (p.formatGroup > 0) {
            QString code = m.captured(p.formatGroup);
            if (formatCodeMap.contains(code))
                format = formatCodeMap[code];
        }
        if (W > 0 && H > 0) break;
    }

    if (W > 0 && H > 0 && format.isEmpty()) {
        if (extensionDefaultMap.contains(ext))
            format = extensionDefaultMap[ext];
    }
}

// ===== 文件夹打开 =====
void CompareViewer::openFolderA() {
    QString dir = FileDialog::getExistingDirectory(this, tr("Open Folder"), "");
    if (dir.isEmpty()) return;
    QDir d(dir);
    QStringList nameFilters = {"*.yuv", "*.data", "*.raw", "*.png", "*.rgb", "*.bmp", "*.jpg"};
    QStringList files = d.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    QStringList fullPaths;
    foreach (const QString &f, files) {
        fullPaths.append(d.absoluteFilePath(f));
    }
    if (fullPaths.isEmpty()) return;
    loadedFilesA = fullPaths;
    matchFiles();
    loadFilesToPanel(0, loadedFilesA);
}

void CompareViewer::openFolderB() {
    QString dir = FileDialog::getExistingDirectory(this, tr("Open Folder"), "");
    if (dir.isEmpty()) return;
    QDir d(dir);
    QStringList nameFilters = {"*.yuv", "*.data", "*.raw", "*.png", "*.rgb", "*.bmp", "*.jpg"};
    QStringList files = d.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    QStringList fullPaths;
    foreach (const QString &f, files) {
        fullPaths.append(d.absoluteFilePath(f));
    }
    if (fullPaths.isEmpty()) return;
    loadedFilesB = fullPaths;
    matchFiles();
    loadFilesToPanel(1, loadedFilesB);
}

// ===== 文件名匹配 =====
void CompareViewer::matchFiles() {
    if (loadedFilesA.isEmpty() || loadedFilesB.isEmpty()) return;

    // 构建 basename 集合
    QSet<QString> namesA, namesB;
    foreach (const QString &f, loadedFilesA) namesA.insert(QFileInfo(f).fileName());
    foreach (const QString &f, loadedFilesB) namesB.insert(QFileInfo(f).fileName());

    // 取交集
    QSet<QString> common = namesA.intersect(namesB);

    if (common.isEmpty()) return;

    // 按交集过滤，保持顺序
    QStringList matchedA, matchedB;
    foreach (const QString &f, loadedFilesA) {
        if (common.contains(QFileInfo(f).fileName())) matchedA.append(f);
    }
    foreach (const QString &f, loadedFilesB) {
        if (common.contains(QFileInfo(f).fileName())) matchedB.append(f);
    }

    loadedFilesA = matchedA;
    loadedFilesB = matchedB;
}

// ===== 加载按钮 =====
void CompareViewer::onLoadA() {
    if (!loadedFilesA.isEmpty()) loadFilesToPanel(0, loadedFilesA, false);
}

void CompareViewer::onLoadB() {
    if (!loadedFilesB.isEmpty()) loadFilesToPanel(1, loadedFilesB, false);
}

// ===== 核心加载 =====
void CompareViewer::loadFilesToPanel(int panelIndex, QStringList filelist, bool autoParse) {
    if (filelist.empty()) return;

    PanelWidgets &pw = (panelIndex == 0) ? pwA : pwB;
    ImgViewer *&viewer = (panelIndex == 0) ? imgViewerA : imgViewerB;
    pw.loadBtn->setEnabled(true);

    // 仅首次加载时自动解析参数
    if (autoParse) {
        QString parsedFmt;
        int parsedW = 0, parsedH = 0;
        parseFilename(filelist.first(), parsedFmt, parsedW, parsedH);

        if (parsedW > 0) pw.widthEdit->setText(QString::number(parsedW));
        if (parsedH > 0) pw.heightEdit->setText(QString::number(parsedH));
        if (!parsedFmt.isEmpty()) pw.formatCombo->setCurrentText(parsedFmt);
    }

    int W = pw.widthEdit->text().toInt();
    int H = pw.heightEdit->text().toInt();
    int startFrame = pw.startFrameEdit->text().toInt();
    int endFrame = pw.endFrameEdit->text().toInt();
    QString format = pw.formatCombo->currentText();

    if (W <= 0 || H <= 0) {
        QMessageBox::critical(this, "Error", tr("Width and Height must be positive"), QMessageBox::Ok);
        return;
    }
    if (format != "PNG") {
        qint64 fileSize = QFileInfo(filelist.first()).size();
        qint64 expected = ImageDecoder::frameSizeBytes(format, W, H);
        if (expected > 0 && fileSize < expected) {
            QMessageBox::critical(this, "Error",
                tr("File size (%1) < expected frame size (%2) for %3 %4x%5.\nPlease check dimensions and format.")
                    .arg(fileSize).arg(expected).arg(format).arg(W).arg(H), QMessageBox::Ok);
            return;
        }
    }

    // 更新文件名标签（显示文件数量）
    pw.filenameLabel->setText(tr("%1 file(s) loaded").arg(filelist.size()));
    pw.filenameLabel->show();

    // 移除占位标签
    QWidget *panel = splitter->widget(panelIndex);
    QLabel *placeholder = panel->findChild<QLabel*>("placeholder");
    if (placeholder) placeholder->hide();

    // 删除旧的 ImgViewer
    if (viewer) {
        delete viewer;
        viewer = nullptr;
    }

    viewer = new ImgViewer("", panel, this);
    QVBoxLayout *panelLayout = qobject_cast<QVBoxLayout*>(panel->layout());
    if (panelLayout) panelLayout->addWidget(viewer, 1);

    // 连接信号
    if (panelIndex == 0) {
        connect(viewer, &ImgViewer::pixelInfoChanged, this, &CompareViewer::onPixelInfoA);
        connect(viewer, &ImgViewer::currentFileChanged, this, &CompareViewer::onFileChangedA);
        connect(viewer, &ImgViewer::viewChanged, this, [this](QPoint p, QImage img) {
            if (syncZoomPanCheck->isChecked() && !syncing && imgViewerB) {
                syncing = true;
                imgViewerB->syncFrom(p, img);
                syncing = false;
            }
        });
    } else {
        connect(viewer, &ImgViewer::pixelInfoChanged, this, &CompareViewer::onPixelInfoB);
        connect(viewer, &ImgViewer::currentFileChanged, this, [this](const QString &name, int frame) {
            pwB.filenameLabel->setText(name + " - " + tr("Frame") + " " + QString::number(frame));
        });
        connect(viewer, &ImgViewer::viewChanged, this, [this](QPoint p, QImage img) {
            if (syncZoomPanCheck->isChecked() && !syncing && imgViewerA) {
                syncing = true;
                imgViewerA->syncFrom(p, img);
                syncing = false;
            }
        });
    }

    bool isSuccess = viewer->setFileList_multithreading(
        filelist, format, W, H, startFrame, endFrame - startFrame + 1);
    if (!isSuccess) {
        QMessageBox::critical(this, "Error", tr("Unsupported format: %1").arg(format), QMessageBox::Ok);
        return;
    }

    viewer->show();
    viewer->fitToWindow();

    if (panelIndex == 0 && imgViewerB) buildSyncMap();
    if (panelIndex == 1 && imgViewerA) buildSyncMap();
}

// ===== 文件打开 =====
void CompareViewer::openFileA() {
    QStringList files = FileDialog::getOpenFileNames(
        this, tr("Open File"), "", "files(*.yuv *.data *.raw *.png *.rgb *.bmp *.jpg)");
    if (files.isEmpty()) return;
    loadedFilesA = files;
    loadFilesToPanel(0, files);
}

void CompareViewer::openFileB() {
    QStringList files = FileDialog::getOpenFileNames(
        this, tr("Open File"), "", "files(*.yuv *.data *.raw *.png *.rgb *.bmp *.jpg)");
    if (files.isEmpty()) return;
    loadedFilesB = files;
    loadFilesToPanel(1, files);
}

// ===== 拖放 =====
void CompareViewer::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void CompareViewer::dropEvent(QDropEvent *event) {
    QStringList filelist;
    foreach (const QUrl &url, event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            QString filepath = url.toLocalFile();
            QFileInfo fi(filepath);
            if (fi.isDir()) {
                // 拖入文件夹：扫描文件
                QDir d(filepath);
                QStringList filters = {"*.yuv", "*.data", "*.raw", "*.png", "*.rgb", "*.bmp", "*.jpg"};
                QStringList files = d.entryList(filters, QDir::Files | QDir::Readable, QDir::Name);
                foreach (const QString &f, files) filelist.append(d.absoluteFilePath(f));
            } else {
                QString suffix = fi.suffix().toLower();
                if (suffix == "yuv" || suffix == "data" || suffix == "raw" || suffix == "png"
                || suffix == "rgb" || suffix == "bmp" || suffix == "jpg") {
                    filelist.append(filepath);
                }
            }
        }
    }
    if (filelist.empty()) return;

    // 判断拖放到哪个面板
    QWidget *child = childAt(event->pos());
    int panelIndex = 0;
    while (child) {
        if (child->property("panelIndex").isValid()) {
            panelIndex = child->property("panelIndex").toInt();
            break;
        }
        for (int i = 0; i < splitter->count(); i++) {
            if (child == splitter->widget(i)) { panelIndex = i; break; }
        }
        child = child->parentWidget();
    }

    // 保存文件列表
    if (panelIndex == 0) loadedFilesA = filelist;
    else loadedFilesB = filelist;

    // 如果两个面板都有文件，尝试匹配
    if (!loadedFilesA.isEmpty() && !loadedFilesB.isEmpty()) matchFiles();

    loadFilesToPanel(panelIndex, (panelIndex == 0) ? loadedFilesA : loadedFilesB);
    event->acceptProposedAction();
}

// ===== 工具栏 =====
void CompareViewer::onSyncZoomPanToggled(bool) {}
void CompareViewer::onFrameSyncToggled(bool checked) {
    if (checked && imgViewerA && imgViewerB) {
        // 开启帧同步时，将 B 面板的帧位置同步到 A 面板
        imgViewerB->syncPosition(imgViewerA);
    }
}

void CompareViewer::onPreviousFrame() {
    if (imgViewerA) imgViewerA->previousImg();
    if (frameSyncCheck->isChecked() && imgViewerB && syncMapAtoB.isEmpty())
        imgViewerB->previousImg();
}

void CompareViewer::onNextFrame() {
    if (imgViewerA) imgViewerA->nextImg();
    if (frameSyncCheck->isChecked() && imgViewerB && syncMapAtoB.isEmpty())
        imgViewerB->nextImg();
}

void CompareViewer::onRotateLeft() {
    if (imgViewerA) imgViewerA->rotateLeft();
    if (frameSyncCheck->isChecked() && imgViewerB) imgViewerB->rotateLeft();
}

void CompareViewer::onRotateRight() {
    if (imgViewerA) imgViewerA->rotateRight();
    if (frameSyncCheck->isChecked() && imgViewerB) imgViewerB->rotateRight();
}

void CompareViewer::onFitWindow() {
    if (imgViewerA) imgViewerA->fitToWindow();
    if (imgViewerB) imgViewerB->fitToWindow();
}

void CompareViewer::onPixelInfoA(int x, int y, int r, int g, int b) {
    pixelInfoLabel->setText(
        QString("[A] x:%1 y:%2 R:%3 G:%4 B:%5").arg(x).arg(y).arg(r).arg(g).arg(b));
}

void CompareViewer::onPixelInfoB(int x, int y, int r, int g, int b) {
    pixelInfoLabel->setText(
        QString("[B] x:%1 y:%2 R:%3 G:%4 B:%5").arg(x).arg(y).arg(r).arg(g).arg(b));
}

void CompareViewer::buildSyncMap() {
    syncMapAtoB.clear();
    if (!imgViewerA || !imgViewerB) return;
    if (loadedFilesA.isEmpty() || loadedFilesB.isEmpty()) return;

    for (const SyncRule &rule : syncRules) {
        QMap<QString, int> keysA, keysB;
        for (int i = 0; i < loadedFilesA.size(); i++) {
            QRegularExpressionMatch m = rule.keyRegex.match(QFileInfo(loadedFilesA[i]).fileName());
            if (m.hasMatch()) keysA[m.captured(rule.keyGroup)] = i;
        }
        for (int i = 0; i < loadedFilesB.size(); i++) {
            QRegularExpressionMatch m = rule.keyRegex.match(QFileInfo(loadedFilesB[i]).fileName());
            if (m.hasMatch()) keysB[m.captured(rule.keyGroup)] = i;
        }
        for (auto it = keysA.begin(); it != keysA.end(); ++it) {
            if (keysB.contains(it.key()))
                syncMapAtoB[it.value()] = keysB[it.key()];
        }
        if (!syncMapAtoB.isEmpty()) break;
    }
}

void CompareViewer::syncBFromA(int globalIndexA) {
    if (!imgViewerB || syncMapAtoB.isEmpty()) return;
    if (!frameSyncCheck->isChecked()) return;
    if (syncMapAtoB.contains(globalIndexA)) {
        imgViewerB->goToGlobalIndex(syncMapAtoB[globalIndexA]);
    }
}

void CompareViewer::onFileChangedA(const QString &name, int frameIndex) {
    pwA.filenameLabel->setText(name + " - " + tr("Frame") + " " + QString::number(frameIndex));
    if (imgViewerA && !syncMapAtoB.isEmpty())
        syncBFromA(imgViewerA->getCurrentGlobalIndex());
}

void CompareViewer::onSyncPlay() {
    if (isSyncPlaying) {
        syncPlayTimer->stop();
        isSyncPlaying = false;
        syncPlayBtn->setText(tr("▶ Play"));
    } else {
        if (!imgViewerA) return;
        int fps = pwA.endFrameEdit->text().toInt();
        if (fps <= 0) fps = 30;
        syncPlayTimer->start(1000 / fps);
        isSyncPlaying = true;
        syncPlayBtn->setText(tr("⏸ Pause"));
    }
}

void CompareViewer::onSyncPlayTimer() {
    if (!imgViewerA) return;
    int cur = imgViewerA->getCurrentGlobalIndex();
    int total = imgViewerA->getTotalFrameCount();
    if (cur >= total - 1) {
        syncPlayTimer->stop();
        isSyncPlaying = false;
        syncPlayBtn->setText(tr("▶ Play"));
        return;
    }
    imgViewerA->nextImg();
    if (frameSyncCheck->isChecked() && imgViewerB && syncMapAtoB.isEmpty())
        imgViewerB->nextImg();
}

void CompareViewer::closeEvent(QCloseEvent *event) {
    if (parentWindow) parentWindow->show();
    event->accept();
}
