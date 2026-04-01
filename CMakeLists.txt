cmake_minimum_required(VERSION 3.16)

project(DspTunerHost LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Network QUIET)
if (Qt6_FOUND)
    set(QT_LIBRARIES Qt6::Widgets Qt6::Network)
    find_package(Qt6 COMPONENTS LinguistTools QUIET)
    # Optional: same-repo component or standalone config (depends on distro packaging).
    find_package(Qt6 COMPONENTS SerialPort QUIET)
    if (NOT TARGET Qt6::SerialPort)
        find_package(Qt6SerialPort QUIET)
    endif()
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
    set(QT_LIBRARIES Qt5::Widgets Qt5::Network)
    find_package(Qt5 COMPONENTS LinguistTools QUIET)
    find_package(Qt5 COMPONENTS SerialPort QUIET)
endif()

if (Qt6_FOUND AND TARGET Qt6::SerialPort)
    list(APPEND QT_LIBRARIES Qt6::SerialPort)
elseif (Qt5_FOUND AND TARGET Qt5::SerialPort)
    list(APPEND QT_LIBRARIES Qt5::SerialPort)
endif()

add_executable(DspTunerHost
    src/main.cpp
    include/DemoView.h
    src/DemoView.cpp
    src/ModuleItem.cpp
    src/ConnectionItem.cpp
    src/PortItem.cpp
    src/Commands.cpp
    src/TitleItem.cpp
    src/MainWindow.cpp
    src/SceneIo.cpp
    src/AudioTypes.cpp
    src/AudioPropertiesDialog.cpp
    src/ModuleTreeWidget.cpp
    src/PatchTransport.cpp
)

if ((Qt6_FOUND AND TARGET Qt6::SerialPort) OR (Qt5_FOUND AND TARGET Qt5::SerialPort))
    target_compile_definitions(DspTunerHost PRIVATE DSP_TUNER_HOST_HAS_SERIAL=1)
    message(STATUS "Qt SerialPort: enabled (TCP + serial patch send)")
else()
    message(STATUS "Qt SerialPort: not found — install qt6-serialport-dev (Qt6) or libqt5serialport5-dev (Qt5), then re-run cmake")
endif()

target_include_directories(DspTunerHost PRIVATE include)
target_link_libraries(DspTunerHost PRIVATE ${QT_LIBRARIES})

# i18n: build qm if LinguistTools is available
set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/DspTunerHost_zh_CN.ts
)

set(DSP_TUNER_QM_READY 0)
if (Qt6_FOUND AND Qt6LinguistTools_FOUND)
    qt_add_translations(DspTunerHost TS_FILES ${TS_FILES})
    set(DSP_TUNER_QM_READY 1)
elseif (Qt5LinguistTools_FOUND)
    qt5_add_translation(QM_FILES ${TS_FILES})
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(DspTunerHost translations)
    set(DSP_TUNER_QM_READY 1)
else()
    # Qt6 without LinguistTools package (common on Debian): call lrelease from PATH.
    find_program(LRELEASE_EXECUTABLE NAMES lrelease-qt6 lrelease lrelease-qt5)
    if (LRELEASE_EXECUTABLE)
        set(QM_GEN "${CMAKE_CURRENT_BINARY_DIR}/DspTunerHost_zh_CN.qm")
        add_custom_command(
            OUTPUT "${QM_GEN}"
            COMMAND ${LRELEASE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/translations/DspTunerHost_zh_CN.ts" -qm "${QM_GEN}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/translations/DspTunerHost_zh_CN.ts"
            VERBATIM
            COMMENT "lrelease DspTunerHost_zh_CN.ts"
        )
        add_custom_target(dsp_tuner_lrelease ALL DEPENDS "${QM_GEN}")
        add_dependencies(DspTunerHost dsp_tuner_lrelease)
        set(DSP_TUNER_QM_READY 1)
    else()
        message(WARNING "lrelease not found; install qt6-l10n-tools (or qt5-default) to build Chinese .qm")
    endif()
endif()

# Copy qm next to the executable so runtime can load it.
if (DSP_TUNER_QM_READY)
    add_custom_command(
        TARGET DspTunerHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:DspTunerHost>/i18n"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/DspTunerHost_zh_CN.qm"
            "$<TARGET_FILE_DIR:DspTunerHost>/i18n/DspTunerHost_zh_CN.qm"
        VERBATIM
    )
endif()
