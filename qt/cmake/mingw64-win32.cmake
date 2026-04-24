# Windows MinGW-w64 交叉编译工具链文件
# 使用方式: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-win32.cmake ..

# 目标系统信息
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 1)

# 指定编译器
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# MinGW 宏定义
add_definitions(-DWIN32_LEAN_AND_MEAN)

# 设置RPATH/INSTALL_RPATH，避免运行时找不到Qt DLL
set(CMAKE_SKIP_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)