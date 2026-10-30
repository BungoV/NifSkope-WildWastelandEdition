# lane OPENCELLFIX2 unit test: the Display groups' masters (src/gl/celllooks.cpp WwGroup) gate their rows' readers.
# Build: copy the tree's .qmake.stash beside the build folder, qmake6 <this> && make && ./cellgroupstest
TEMPLATE = app
TARGET = cellgroupstest
QT += core
CONFIG += c++20 console
CONFIG -= app_bundle

ROOT = $$PWD/../..
INCLUDEPATH += $$ROOT/src $$ROOT/src/gl
!*msvc*:QMAKE_CXXFLAGS += -march=haswell
# lane REDRETIRE1: the red build (qmake6 <this> REDTWIN=1) brings back the retired reds as twins (src/wwredtwin.h);
# run it with REDTWIN=CELLGROUP=<value>. tests/cellgroups/run.sh builds and runs both.
!isEmpty(REDTWIN): DEFINES += WW_REDTWIN

SOURCES += cellgroupstest.cpp $$ROOT/src/gl/celllooks.cpp $$ROOT/src/wwswitch.cpp
