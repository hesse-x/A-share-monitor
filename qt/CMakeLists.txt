cmake_minimum_required(VERSION 3.16)

# Project name and version
project(StockMonitor VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror -Wno-unused-parameter)

set(SANITIZER "" CACHE STRING "启用的Sanitizer类型，逗号分隔：addr(地址)/thread(线程)/ub(未定义行为)")
if(SANITIZER)
    # 仅支持GCC/Clang（MSVC Sanitizer参数不同，需单独适配）
    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
        message(FATAL_ERROR "Sanitizer仅支持GCC/Clang编译器！当前编译器：${CMAKE_CXX_COMPILER_ID}")
    endif()

    # 拆分逗号分隔的Sanitizer列表
    string(REPLACE "," ";" SANITIZER_LIST ${SANITIZER})
    # 去重（避免用户重复传参，如 -DSANITIZER=addr,addr）
    list(REMOVE_DUPLICATES SANITIZER_LIST)

    # 定义Sanitizer映射表：类型 -> 编译/链接标志
    set(SANITIZER_FLAGS_MAP
        "addr;address"
        "thread;thread"
        "ub;undefined"
    )
# string(SANITIZER_FLAG_STR "address,undefined")
    add_compile_options(
        -fsanitize=address,undefined    # 启用指定Sanitizer
        -g                                   # 生成调试信息（必须，否则无行号）
        -O1                                  # 低优化（O2/O3会掩盖错误，O0也可）
        -fno-omit-frame-pointer              # 保留栈帧，显示完整调用栈
    )
    add_link_options(
        -fsanitize=address,undefined
    )


endif()

# Flag to mark if Qt is found
set(QT_FOUND FALSE)

# 1. First try to find Qt6
find_package(Qt6 COMPONENTS Core Gui Widgets Network QUIET)
if(Qt6_FOUND)
    set(QT_FOUND TRUE)
    message(STATUS "Found Qt6: ${Qt6_VERSION}")
    # Qt6 module reference: Qt6::Core, Qt6::Gui, etc.
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
else()
    # 2. If Qt6 not found, try to find Qt5
    find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)
    if(Qt5_FOUND)
        set(QT_FOUND TRUE)
        message(STATUS "Found Qt5: ${Qt5_VERSION}")
        # Qt5 module reference: Qt5::Core, Qt5::Gui, etc.
        set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network)
    endif()
endif()

# Automatically handle Qt's moc, uic, rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    main.cpp
    widget.cpp
    config_parser.cpp
    config_dialog.cpp
    config_dialog.ui
# display strategy
    display_mode.cpp
    line_chart_mode.cpp
    data_only_mode.cpp
)

add_library(Utils OBJECT
    logger.cpp
    utils.cpp
)

add_library(Stock OBJECT
# stock fetcher
    stock_fetcher.cpp
    sina_fetcher.cpp
    future_fetcher.cpp
    random_fetcher.cpp
    stock.cpp
)
target_link_libraries(Stock PRIVATE ${QT_LIBRARIES} Utils)

# Create executable
add_executable(StockMonitor ${SOURCES})

# Link Qt libraries
target_link_libraries(StockMonitor PRIVATE ${QT_LIBRARIES} Utils Stock)

# Set as GUI application on Windows (no console window)
if(WIN32)
    set_target_properties(StockMonitor PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()
