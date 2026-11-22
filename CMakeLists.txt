cmake_minimum_required(VERSION 3.16)
project(PhotoStudio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# підтримуються Qt 6 та Qt 5.14+
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql PrintSupport Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql PrintSupport Svg)

if(MSVC)
    set(WARNINGS /W4)
else()
    set(WARNINGS -Wall -Wextra -Wpedantic)
endif()

# ядро: предметна логіка та доступ до даних (без графічного інтерфейсу)
add_library(studio_core STATIC
    src/core/pricing.cpp      src/core/pricing.h
    src/core/scheduling.cpp   src/core/scheduling.h
    src/core/database.cpp     src/core/database.h
    src/core/session.cpp      src/core/session.h
)
target_include_directories(studio_core PUBLIC src)
target_link_libraries(studio_core PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)
target_compile_options(studio_core PRIVATE ${WARNINGS})

set(UI_SOURCES
    src/main.cpp
    src/core/exporter.cpp       src/core/exporter.h
    src/ui/theme.cpp            src/ui/theme.h
    src/ui/widgets.cpp          src/ui/widgets.h
    src/ui/logindialog.cpp      src/ui/logindialog.h
    src/ui/mainwindow.cpp       src/ui/mainwindow.h
    src/ui/dashboardpage.cpp    src/ui/dashboardpage.h
    src/ui/schedulewidget.cpp   src/ui/schedulewidget.h
    src/ui/schedulepage.cpp     src/ui/schedulepage.h
    src/ui/bookingspage.cpp     src/ui/bookingspage.h
    src/ui/bookingdialog.cpp    src/ui/bookingdialog.h
    src/ui/rentalspage.cpp      src/ui/rentalspage.h
    src/ui/rentaldialog.cpp     src/ui/rentaldialog.h
    src/ui/catalogpage.cpp      src/ui/catalogpage.h
    src/ui/clientspage.cpp      src/ui/clientspage.h
    src/ui/reportspage.cpp      src/ui/reportspage.h
    src/ui/barchart.cpp         src/ui/barchart.h
    src/ui/userspage.cpp        src/ui/userspage.h
    src/ui/helpdialog.cpp       src/ui/helpdialog.h
    resources/resources.qrc
)

if(WIN32)
    list(APPEND UI_SOURCES resources/app.rc)
endif()

add_executable(PhotoStudio WIN32 MACOSX_BUNDLE ${UI_SOURCES})
target_link_libraries(PhotoStudio PRIVATE studio_core
    Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::PrintSupport Qt${QT_VERSION_MAJOR}::Svg)
target_compile_options(PhotoStudio PRIVATE ${WARNINGS})

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE app.icns)
    set_source_files_properties(resources/app.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(PhotoStudio PRIVATE resources/app.icns)
    set_target_properties(PhotoStudio PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "PhotoStudio"
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()

# автоматичні тести (Qt Test): ctest --test-dir build
option(BUILD_TESTS "Збирати тести" ON)
if(BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    enable_testing()
    add_executable(tst_core tests/tst_core.cpp resources/resources.qrc)
    target_link_libraries(tst_core PRIVATE studio_core Qt${QT_VERSION_MAJOR}::Test)
    target_compile_options(tst_core PRIVATE ${WARNINGS})
    add_test(NAME tst_core COMMAND tst_core)
endif()
