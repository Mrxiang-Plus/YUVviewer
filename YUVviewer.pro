###############################################################################
#                                                                             #
# YUVviewer 工程文件                                                           #  
#                                                                             # 
###############################################################################
win32:{
    include(partform_win32.pri)
}

unix:{
    include(partform_unix.pri)
}

# 定义版本号路径
YUVVIEWER_VERSION="$$cat(./version.txt)"

###############################################################################

!versionAtLeast(QT_VERSION, 5.15.0) {
    message("Cannot use Qt $$QT_VERSION")
    error("Use Qt 5.15.0 or newer")
}

# 定义需要的Qt组件
QT       += core gui
QT       += xml svg
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 编译配置
TARGET = YUVviewer
TEMPLATE = app
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += APP_VERSION="\\\"V$${YUVVIEWER_VERSION}\\\""
CONFIG += c++11

# 源文件配置
INCLUDEPATH += \
        $$PWD \
        $$PWD/src 

SOURCES += \
        src/YUVviewer.cpp \
        src/ImgViewer.cpp \
        src/ImgExport.cpp \
        src/YUVdecoder.cpp \
        src/configFile.cpp \
        src/CompareViewer.cpp

HEADERS += \
        src/YUVviewer.h \
        src/ImgViewer.h \
        src/ImgExport.h \
        src/YUVdecoder.h \
        src/filedialog.h \
        src/configFile.h \
        src/CompareViewer.h

FORMS += \
        src/UI_YUVviewer.ui \
        src/UI_ImgExport.ui \
        src/UI_ImgViewer.ui

RESOURCES += \
        src/img.qrc

TRANSLATIONS += \
    lang/yuvviewer_zh_CN.ts \
    lang/yuvviewer_ja_JP.ts \
    lang/yuvviewer_en_US.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 输出配置
build_type =
CONFIG(debug, debug|release) {
    build_type = build_debug
} else {
    build_type = build_release
}

DESTDIR     = $$build_type/out
OBJECTS_DIR = $$build_type/obj
MOC_DIR     = $$build_type/moc
RCC_DIR     = $$build_type/rcc
UI_DIR      = $$build_type/ui

# 平台配置
win32:{
    INCLUDEPATH += $${OPENCV_DIR}\include\opencv2 \
                    $${OPENCV_DIR}\include

    LIBS += $${OPENCV_DIR}\x64\mingw\lib\libopencv_*.a

    VERSION = $${YUVVIEWER_VERSION}.000
    RC_ICONS = "img\ico.ico"
    QMAKE_TARGET_PRODUCT = "YUVviewer"
    QMAKE_TARGET_DESCRIPTION = "YUVviewer based on Qt $$[QT_VERSION]"
    QMAKE_TARGET_COPYRIGHT = "GNU General Public License v3.0"

    build_info_gen = c:/Windows/system32/WindowsPowerShell/v1.0/powershell.exe -ExecutionPolicy Bypass -NoLogo -NoProfile -File $$PWD/tools/generate_info.ps1 > $$PWD/build_info.inc
}

unix:!macx:{
    QMAKE_RPATHDIR=$ORIGIN
    QMAKE_LFLAGS += -no-pie

    INCLUDEPATH += -I $${OPENCV_DIR}/include/opencv4
    DEPENDPATH +=$${OPENCV_DIR}/include/opencv4

    INCLUDEPATH += -I $${OPENCV_DIR}/include
    DEPENDPATH +=$${OPENCV_DIR}/include

    LIBS += -L $${OPENCV_DIR}/lib/ -lopencv_imgproc
    LIBS += -L $${OPENCV_DIR}/lib/ -lopencv_imgcodecs
    LIBS += -L $${OPENCV_DIR}/lib/ -lopencv_core
    LIBS += -L/home/mi/snap/anaconda3/lib -lstdc++

    build_info_gen = cd $$PWD && ./tools/generate_info.sh > build_info.inc
}

macx:{
    QMAKE_RPATHDIR=$ORIGIN
    ICON = "img/ico.icns"

    INCLUDEPATH += -I $${OPENCV_DIR}/include/opencv4
    DEPENDPATH +=$${OPENCV_DIR}/include/opencv4

    INCLUDEPATH += -I $${OPENCV_DIR}/include
    DEPENDPATH +=$${OPENCV_DIR}/include

    LIBS += -L $${OPENCV_DIR}/lib/ -lopencv_imgproc
    LIBS += -L $${OPENCV_DIR}/lib/ -lopencv_imgcodecs
    LIBS += -L $${OPENCV_DIR}/lib/ -lopencv_core

    build_info_gen = cd $$PWD && ./tools/generate_info.sh > build_info.inc
}

# 在 qmake 阶段生成 build_info.inc，避免并行 make 时重写该文件与编译竞态
win32:system(cmd /c $$build_info_gen)
unix:system($$build_info_gen)

###############################################################################
