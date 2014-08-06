#-------------------------------------------------
#
# Project created by QtCreator 2013-01-06T03:07:13
#
#-------------------------------------------------

QT       -= core gui

PROJECT(i2psam)
CONFIG += staticlib

QMAKE_CXXFLAGS += -Wall

SOURCES += i2psam.cpp

HEADERS += i2psam.h
unix:!symbian {
    maemo5 {
SET(        target.path /opt/usr/lib)
    } else {
SET(        target.path /usr/lib)
    }
    INSTALLS += target
}
ADD_LIBRARY(i2psam ${i2psam_sources})
