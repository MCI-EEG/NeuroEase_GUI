cmake_minimum_required(VERSION 3.16)

project(NeuroEase_GUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 / Qt5 Kompatibilität + benötigte Module
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS
    Widgets
    PrintSupport
    Network
    SerialPort
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Widgets
    PrintSupport
    Network
    SerialPort
)

# Alle Projektquellen
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    qcustomplot.cpp
    qcustomplot.h
    zoomablegraphicsview.cpp
    zoomablegraphicsview.h
    DummyDataSource.h
    DummyDataSource.cpp
    RealDataSource.h
    RealDataSource.cpp
    AbstractDataSource.h
    FileDataSource.h
    FileDataSource.cpp
    electrodemap.h
    electrodemap.cpp
    DataProcessingQt.h
    DataProcessingQt.cpp
)
#test
# Executable erzeugen
add_executable(NeuroEase_GUI ${PROJECT_SOURCES}
    DataProcessingQt.h
    DataProcessingQt.cpp)

target_link_libraries(NeuroEase_GUI PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::SerialPort
)

# Windows/Mac App Bundle Einstellungen
set_target_properties(NeuroEase_GUI PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Optional: Installation (kann ignoriert werden)
include(GNUInstallDirs)
install(TARGETS NeuroEase_GUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt6 Spezifikum
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NeuroEase_GUI)
endif()
