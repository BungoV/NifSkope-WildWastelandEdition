include(NifSkope.pro)

# Build the full application source set with the deterministic Rigging harness
# replacing the normal GUI entry point. Keeping a separate intermediate tree
# prevents test builds from reusing or overwriting production objects.
TARGET = RiggingIntegration
CONFIG += console
CONFIG -= windows

SOURCES -= src/main.cpp
SOURCES += tests/rigging/rigging_integration.cpp

RC_FILE =
DESTDIR = $$PWD/release
INTERMEDIATE = $$PWD/GeneratedFiles/RiggingIntegration
UI_DIR = $$INTERMEDIATE/.ui
MOC_DIR = $$INTERMEDIATE/.moc
RCC_DIR = $$INTERMEDIATE/.qrc
OBJECTS_DIR = $$INTERMEDIATE/.obj

