cmake_minimum_required(VERSION 3.24)

project(ZVRNK_CANTool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(ZVRNK_CANTool
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/theme.cpp
    src/theme.h
    src/localization.cpp
    src/localization.h
    src/nmea2000.cpp
    src/nmea2000.h
    src/session_widget.cpp
    src/session_widget.h
    src/rx_frame_table_model.cpp
    src/rx_frame_table_model.h
    src/usb_can_service.cpp
    src/usb_can_service.h
    src/control_can_native.cpp
    src/control_can_native.h
    src/can_types.h
    src/formula_evaluator.cpp
    src/formula_evaluator.h
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/logo.png")
    target_sources(ZVRNK_CANTool PRIVATE assets/resources.qrc)
else()
    message(STATUS "assets/logo.png not found. Add it and rerun CMake to enable the app logo.")
endif()

target_link_libraries(ZVRNK_CANTool PRIVATE Qt6::Widgets)

if(WIN32)
    target_compile_definitions(ZVRNK_CANTool PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

set(DEFAULT_CONTROLCAN_DLL_PATH "")
if(DEFINED ENV{CONTROLCAN_DLL_PATH})
    set(DEFAULT_CONTROLCAN_DLL_PATH "$ENV{CONTROLCAN_DLL_PATH}")
endif()
set(CONTROLCAN_DLL_PATH "${DEFAULT_CONTROLCAN_DLL_PATH}" CACHE FILEPATH "Optional path to vendor ControlCAN.dll")
if(CONTROLCAN_DLL_PATH AND EXISTS "${CONTROLCAN_DLL_PATH}")
    add_custom_command(TARGET ZVRNK_CANTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTROLCAN_DLL_PATH}"
            "$<TARGET_FILE_DIR:ZVRNK_CANTool>/ControlCAN.dll"
        COMMENT "Copying ControlCAN.dll beside the executable")
elseif(CONTROLCAN_DLL_PATH)
    message(WARNING "CONTROLCAN_DLL_PATH does not exist: ${CONTROLCAN_DLL_PATH}")
else()
    message(WARNING "CONTROLCAN_DLL_PATH is not set. Copy ControlCAN.dll beside the executable before running.")
endif()
