/* lane STARTSPEED1: the precompiled header (NifSkope.pro PRECOMPILED_HEADER; qmake nopch=1 builds without it).
 * A full build spent ~11 s an object, nearly all of it parsing the same Qt and standard headers (the slowest
 * object was 2.4 % of the summed time): these are parsed once. Only system headers: a project header here
 * would rebuild everything on each edit of it. The C sources (libfo76utils) get an empty one. */
#ifdef __cplusplus
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <QtCore>
// QtGui/QtWidgets by class, not the module headers: those pull qopengl.h (GL headers) into every source
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QWidget>
#endif
