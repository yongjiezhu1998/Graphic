cmake_minimum_required(VERSION 3.16)

project(GraphicsDemo LANGUAGES CXX)

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

add_executable(GraphicsDemo
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
    target_compile_definitions(GraphicsDemo PRIVATE GRAPHICS_DEMO_HAS_SERIAL=1)
    message(STATUS "Qt SerialPort: enabled (TCP + serial patch send)")
else()
    message(STATUS "Qt SerialPort: not found — install qt6-serialport-dev (Qt6) or libqt5serialport5-dev (Qt5), then re-run cmake")
endif()

target_include_directories(GraphicsDemo PRIVATE include)
target_link_libraries(GraphicsDemo PRIVATE ${QT_LIBRARIES})

# i18n: build qm if LinguistTools is available
set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/GraphicsDemo_zh_CN.ts
)

if (Qt6_FOUND AND Qt6LinguistTools_FOUND)
    qt_add_translations(GraphicsDemo TS_FILES ${TS_FILES})
elseif (Qt5LinguistTools_FOUND)
    qt5_add_translation(QM_FILES ${TS_FILES})
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(GraphicsDemo translations)
endif()

# Copy qm next to the executable so runtime can load it.
# Multi-config generators (MSVC) will place the exe in build/<Config>/, so copy after build.
add_custom_command(TARGET GraphicsDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GraphicsDemo>/i18n"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/GraphicsDemo_zh_CN.qm"
        "$<TARGET_FILE_DIR:GraphicsDemo>/i18n/GraphicsDemo_zh_CN.qm"
    VERBATIM
)
