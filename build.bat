@echo off
echo 正在构建GXTStudio Qt项目...

REM 检查是否在正确的目录
if not exist "CMakeLists.txt" (
    echo 错误: 未找到CMakeLists.txt文件
    echo 请确保在项目根目录中运行此脚本
    pause
    exit /b 1
)

REM 创建构建目录
if not exist "build" mkdir build
cd build

REM 运行CMake配置
echo 正在配置项目...
cmake .. -G "Visual Studio 16 2019" -A x64
if %ERRORLEVEL% neq 0 (
    echo CMake配置失败
    pause
    exit /b 1
)

REM 构建项目
echo 正在构建项目...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo 构建失败
    pause
    exit /b 1
)

echo 构建成功！
echo 可执行文件位置: build\Release\GXTStudio.exe

pause