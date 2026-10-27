# lane STREAMMEM1: the stream walk's geometry cache on the real driver (green flat, "red" grows)
QT += gui opengl
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = streammem
ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src/gl
SOURCES += streammem.cpp
LIBS += -lpsapi
