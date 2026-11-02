# lane PERFFIX5: the stream walk's file textures on the real driver (green recycles storage, "red" deletes)
QT += gui opengl
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = texrecycle_gl
ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src/gl
SOURCES += texrecycle_gl.cpp
LIBS += -lpsapi
