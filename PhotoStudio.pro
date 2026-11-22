# Збирання через qmake (Qt Creator). Основний спосіб - CMakeLists.txt.
QT += core gui widgets sql printsupport svg

CONFIG += c++17
TARGET = PhotoStudio
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS
!msvc: QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
VERSION = 1.0.0

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/core/database.cpp \
    src/core/pricing.cpp \
    src/core/scheduling.cpp \
    src/core/session.cpp \
    src/core/exporter.cpp \
    src/ui/theme.cpp \
    src/ui/widgets.cpp \
    src/ui/logindialog.cpp \
    src/ui/mainwindow.cpp \
    src/ui/dashboardpage.cpp \
    src/ui/schedulewidget.cpp \
    src/ui/schedulepage.cpp \
    src/ui/bookingspage.cpp \
    src/ui/bookingdialog.cpp \
    src/ui/rentalspage.cpp \
    src/ui/rentaldialog.cpp \
    src/ui/catalogpage.cpp \
    src/ui/clientspage.cpp \
    src/ui/reportspage.cpp \
    src/ui/barchart.cpp \
    src/ui/userspage.cpp \
    src/ui/helpdialog.cpp

HEADERS += \
    src/core/database.h \
    src/core/pricing.h \
    src/core/scheduling.h \
    src/core/session.h \
    src/core/exporter.h \
    src/ui/theme.h \
    src/ui/widgets.h \
    src/ui/logindialog.h \
    src/ui/mainwindow.h \
    src/ui/dashboardpage.h \
    src/ui/schedulewidget.h \
    src/ui/schedulepage.h \
    src/ui/bookingspage.h \
    src/ui/bookingdialog.h \
    src/ui/rentalspage.h \
    src/ui/rentaldialog.h \
    src/ui/catalogpage.h \
    src/ui/clientspage.h \
    src/ui/reportspage.h \
    src/ui/barchart.h \
    src/ui/userspage.h \
    src/ui/helpdialog.h

RESOURCES += resources/resources.qrc

win32: RC_ICONS = resources/app.ico
macx: ICON = resources/app.icns
