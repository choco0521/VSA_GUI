cmake_minimum_required(VERSION 3.21)

project(VSA_GUI
    VERSION 0.1.0
    DESCRIPTION "Video Stream Analyzer GUI (Elecard StreamEye style, Qt 6)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt-specific setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Prefer native macOS look
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Charts)

qt_standard_project_setup()

set(VSA_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h

    src/model/StreamInfo.h
    src/model/StreamInfo.cpp
    src/model/FrameData.h
    src/model/FrameData.cpp
    src/model/CodingUnit.h
    src/model/CodingUnit.cpp
    src/model/MockDataProvider.h
    src/model/MockDataProvider.cpp

    src/views/ChartTabWidget.h
    src/views/ChartTabWidget.cpp
    src/views/BarChartView.h
    src/views/BarChartView.cpp
    src/views/AreaChartView.h
    src/views/AreaChartView.cpp
    src/views/ThumbnailsView.h
    src/views/ThumbnailsView.cpp
    src/views/VideoPreviewWidget.h
    src/views/VideoPreviewWidget.cpp
    src/views/MotionVectorView.h
    src/views/MotionVectorView.cpp
    src/views/StreamInfoTree.h
    src/views/StreamInfoTree.cpp
    src/views/CodingUnitTree.h
    src/views/CodingUnitTree.cpp
    src/views/BitrateCurveView.h
    src/views/BitrateCurveView.cpp

    src/widgets/MainToolbar.h
    src/widgets/MainToolbar.cpp
    src/widgets/ViewModeSelector.h
    src/widgets/ViewModeSelector.cpp
)

qt_add_executable(vsa_gui
    ${VSA_SOURCES}
    resources/resources.qrc
)

target_include_directories(vsa_gui PRIVATE src)

target_link_libraries(vsa_gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Charts
)

# Produce a proper .app bundle on macOS
set_target_properties(vsa_gui PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "VSA GUI"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.vsa_gui"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# Silence harmless deprecation warnings on newer Qt
target_compile_definitions(vsa_gui PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)
