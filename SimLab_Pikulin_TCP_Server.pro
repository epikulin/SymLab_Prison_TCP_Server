#-------------------------------------------------
#
# Project created by QtCreator 2019-12-12T14:23:52
#
#-------------------------------------------------
QT += charts
QT       += core gui
QT += network widgets
requires(qtConfig(filedialog))

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = untitled
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui
