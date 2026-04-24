基于QT的小工具，可以监控指定财富密码的实时变化，并显示在屏幕上

```shell
mkdir build
cd build
cmake ../
make
cd ../
./build/StockMonitor stock.config
```

```shell
mkdir build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../
iwyu_tool.py -p  . -- -Xiwyu --mapping_file=../iwyu.imp
```

```shell
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=addr,ub ../
```

```cross compile
export QT_CROSS_WIN=/home/cambricon/qt5-win64-cross
cmake ../ -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64-win32.cmake
```
