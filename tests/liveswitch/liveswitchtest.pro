# LIVESET1 unit test: a live set rebuilds a reader only when its value moved; the stage table classifies the converted
# switches live. Build: copy the tree's .qmake.stash beside the build folder, qmake6 <this> && make && ./liveswitchtest
TEMPLATE = app
TARGET = liveswitchtest
QT += core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src
!*msvc*:QMAKE_CXXFLAGS += -march=haswell

SOURCES += liveswitchtest.cpp $$ROOT/src/wwswitch.cpp $$ROOT/src/gl/fogterrain.cpp
