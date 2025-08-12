cmake_minimum_required(VERSION 3.16)

# Project name and version
project(StockMonitor VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Flag to mark if Qt is found
set(QT_FOUND FALSE)

# 1. First try to find Qt6
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(Qt6_FOUND)
    set(QT_FOUND TRUE)
    message(STATUS "Found Qt6: ${Qt6_VERSION}")
    # Qt6 module reference: Qt6::Core, Qt6::Gui, etc.
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets)
else()
    # 2. If Qt6 not found, try to find Qt5
    find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
    if(Qt5_FOUND)
        set(QT_FOUND TRUE)
        message(STATUS "Found Qt5: ${Qt5_VERSION}")
        # Qt5 module reference: Qt5::Core, Qt5::Gui, etc.
        set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets)
    endif()
endif()
find_package(CURL REQUIRED)

# Automatically handle Qt's moc, uic, rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    main.cpp
    widget.cpp
    data.cpp
    stock_fetcher.cpp
)

# Create executable
add_executable(StockMonitor ${SOURCES})

# Link Qt libraries
target_link_libraries(StockMonitor PRIVATE ${QT_LIBRARIES} curl)

# Set as GUI application on Windows (no console window)
if(WIN32)
    set_target_properties(StockMonitor PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()

# Set no dock icon on macOS
if(APPLE)
    set_target_properties(StockMonitor PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    )
endif()
