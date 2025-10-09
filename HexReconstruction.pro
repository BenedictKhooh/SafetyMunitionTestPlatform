QT       += core gui \
            widgets  \
            opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
win32: LIBS += -lopengl32

SRC_DIR = src
INCLUDEPATH  += \
     $$PWD/$$SRC_DIR

#INCLUDEPATH += $$PWD/building_blocks_method

SOURCES += \
$$SRC_DIR/app/main.cpp \
    $$SRC_DIR/app/mainwindow.cpp \
    $$SRC_DIR/app/glwidget.cpp \
    src/app/commandline.cpp

HEADERS += \
$$SRC_DIR/app/mainwindow.h \
    $$SRC_DIR/app/glwidget.h \
    $$SRC_DIR/core/reconstruction/reconstruction_engine.h \
    src/app/commandline.h \
    src/core/algorithm/Algorithm.h \
    src/core/common/STL_reader.h \
    src/core/common/common_types.h \
    src/core/eos/eos.h \
    src/core/generation/cubic_block.h \
    src/core/generation/cylinder_projection.h \
    src/core/generation/revolver_block.h \
    src/core/generation/sampling.h \
    src/core/generation/sphere_projection.h \
    src/core/materials/materials.h \
    src/core/reconstruction/instance_builder.h \
    src/core/reconstruction/instance_modification.h \
    src/core/reconstruction/mesh_refinement.h

FORMS += \
#    mainwindow.ui
    $$SRC_DIR/app/ui/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
