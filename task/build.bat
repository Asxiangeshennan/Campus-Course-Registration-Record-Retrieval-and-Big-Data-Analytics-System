@echo off
chcp 65001 >nul
cd /d "%~dp0"

set "FILENAME=%~nx1"

if "%FILENAME%"=="data_generator.c" goto BuildGenerator
goto BuildMain

:BuildGenerator
echo ==========================================
echo [编译] 数据生成器 [独立模式 -DSTANDALONE]
echo ==========================================

ren src\main.c main.c.bak 2>nul

gcc -std=c11 -g -Wall -DSTANDALONE -finput-charset=UTF-8 -fexec-charset=UTF-8 -I inc src\*.c -o data_generator.exe -lm

:: ✅ 修复Bug1：在ren恢复之前，立即保存gcc的编译结果
set GCC_RESULT=%errorlevel%

ren src\main.c.bak main.c 2>nul

if %GCC_RESULT% equ 0 (
    echo.
    echo [✓ 编译成功] 启动数据生成器...
    echo.
    :: ✅ 修复Bug2：用 & 代替 &&，确保程序崩溃时也能暂停看到错误信息
    start cmd /k "chcp 65001 >nul & cd /d "%~dp0" & data_generator.exe & echo. & echo ======================================== & echo 程序已结束（退出码: %errorlevel%） & echo ========================================"
) else (
    echo.
    echo [✗ 编译失败！] 请检查上方的错误信息。
    echo.
    pause
)
exit /b

:BuildMain
echo ==========================================
echo [编译] 主程序 [通配符模式 src\*.c]
echo ==========================================

gcc -std=c11 -g -Wall -finput-charset=UTF-8 -fexec-charset=UTF-8 -I inc src\*.c -o main.exe -lm

set GCC_RESULT=%errorlevel%

if %GCC_RESULT% equ 0 (
    echo.
    echo [✓ 编译成功] 启动主程序...
    echo.
    start cmd /k "chcp 65001 >nul & cd /d "%~dp0" & main.exe & echo. & echo ======================================== & echo 程序已结束（退出码: %errorlevel%） & echo ========================================"
) else (
    echo.
    echo [✗ 编译失败！] 请检查上方的错误信息。
    echo.
    pause
)
exit /b