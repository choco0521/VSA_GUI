cmake_minimum_required(VERSION 3.21)

project(VSA_GUI
    VERSION 0.1.0
    DESCRIPTION "Video Stream Analyzer GUI (Elecard StreamEye style, Qt 6)"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VSA_BUILD_TESTS "Build vsa_codec unit tests (doctest)" ON)
option(VSA_BUILD_TOOLS "Build headless CLI tools (vsa_parse)" ON)

# Qt-specific setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Prefer native macOS look
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# ---------------------------------------------------------------------------
# vsa_codec — pure C11 bitstream parsing library. No Qt, no C++ STL.
# Consumed by the Qt GUI (src/model/H264DataProvider), by the vsa_parse CLI,
# and by the doctest-based unit tests.
# ---------------------------------------------------------------------------
set(VSA_CODEC_SOURCES
    src/codec/codec_version.h
    src/codec/codec_version.c
    src/codec/bit_reader.h
    src/codec/bit_reader.c
    src/codec/annexb.h
    src/codec/annexb.c
    src/codec/rbsp.h
    src/codec/rbsp.c
    src/codec/h264_nalu.h
    src/codec/h264_sps.h
    src/codec/h264_sps.c
    src/codec/h264_slice_header.h
    src/codec/h264_slice_header.c
)

add_library(vsa_codec STATIC ${VSA_CODEC_SOURCES})
target_include_directories(vsa_codec PUBLIC src)
target_compile_features(vsa_codec PRIVATE c_std_11)
set_target_properties(vsa_codec PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    LINKER_LANGUAGE C
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vsa_codec PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Qt GUI target. Optional when cross-compiling just the codec library, so
# we skip it gracefully if Qt is not installed on the build machine.
# ---------------------------------------------------------------------------
find_package(Qt6 6.4 QUIET COMPONENTS Core Gui Widgets Charts)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found — skipping vsa_gui target. vsa_codec, "
                   "vsa_tests and vsa_parse will still build.")
endif()

if(Qt6_FOUND)

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

target_link_libraries(vsa_gui PRIVATE vsa_codec)

# Silence harmless deprecation warnings on newer Qt
target_compile_definitions(vsa_gui PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

endif() # Qt6_FOUND

# ---------------------------------------------------------------------------
# Headless CLI: vsa_parse (C++, links against vsa_codec via extern "C").
# Always built because it has no Qt dependency and is what CI diffs against
# the JM reference decoder's trace output.
# ---------------------------------------------------------------------------
if(VSA_BUILD_TOOLS)
    add_executable(vsa_parse src/tools/vsa_parse.cpp)
    target_link_libraries(vsa_parse PRIVATE vsa_codec)
endif()

# ---------------------------------------------------------------------------
# Unit tests (doctest, single-header, fetched at configure time). Run with
# `ctest --test-dir build --output-on-failure`.
# ---------------------------------------------------------------------------
if(VSA_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    enable_testing()

    set(VSA_TEST_SOURCES
        tests/test_main.cpp
        tests/test_smoke.cpp
        tests/test_bitreader.cpp
        tests/test_annexb.cpp
        tests/test_annexb_large.cpp
        tests/test_rbsp.cpp
        tests/test_sps.cpp
        tests/test_slice_header.cpp
    )
    add_executable(vsa_tests ${VSA_TEST_SOURCES})
    target_link_libraries(vsa_tests PRIVATE vsa_codec doctest::doctest)
    target_compile_definitions(vsa_tests PRIVATE
        VSA_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
    )

    # Run ctest from the source tree so relative fixture paths in the
    # smoke harness (and any future JM trace comparison step) resolve.
    add_test(NAME vsa_tests
             COMMAND vsa_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
