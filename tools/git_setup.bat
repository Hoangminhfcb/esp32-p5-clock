@echo off
rem =====================================================================
rem  git_setup.bat - Thiet lap git cho du an (chay 1 lan)
rem    1. Xoa file trung lap / khong can thiet (tools\clean_old_files.bat)
rem    2. Tao main\config\secrets.h neu chua co
rem    3. git init + dua thu vien HUB75 vao dang submodule
rem    4. Commit dau tien
rem  Chay lai nhieu lan cung khong sao.
rem =====================================================================
setlocal
cd /d "%~dp0.."
echo.
echo === Thiet lap git cho esp32-p5-hub75 ===
echo Thu muc: %CD%
echo.

where git >nul 2>nul
if errorlevel 1 (
    echo [LOI] Chua cai Git. Tai tai: https://git-scm.com/download/win
    pause
    exit /b 1
)

rem ---- 1. Xoa file trung lap / khong can thiet -----------------------
echo [1/4] Don file cu...
call "%~dp0clean_old_files.bat" nopause

rem ---- 2. secrets.h --------------------------------------------------
echo [2/4] Kiem tra main\config\secrets.h...
if not exist "main\config\secrets.h" copy /y "main\config\secrets.example.h" "main\config\secrets.h" >nul

rem ---- 3. git init + submodule ----------------------------------------
echo [3/4] Khoi tao git...
if not exist ".git" (
    git init
    git symbolic-ref HEAD refs/heads/main
)

git config user.name >nul 2>nul
if errorlevel 1 goto ask_name
:after_name
git config user.email >nul 2>nul
if errorlevel 1 goto ask_email
:after_email

git ls-files --stage components/ESP32-HUB75-MatrixPanel-DMA | findstr "^160000" >nul
if errorlevel 1 (
    git submodule add https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA components/ESP32-HUB75-MatrixPanel-DMA
)

rem ---- 4. Commit -------------------------------------------------------
echo [4/4] Commit...
git add -A
git commit -m "Khoi tao du an: cau truc config/hal/gfx/app/net, web UI, font tieng Viet"

echo.
echo === XONG ===
echo Dua len GitHub: tao repo TRONG tren github.com roi chay:
echo    git remote add origin https://github.com/TEN-BAN/esp32-p5-hub75.git
echo    git push -u origin main
echo.
pause
exit /b 0

:ask_name
set /p GIT_NAME=Ten cua ban (hien trong commit): 
git config --global user.name "%GIT_NAME%"
goto after_name

:ask_email
set /p GIT_EMAIL=Email cua ban (hien trong commit): 
git config --global user.email "%GIT_EMAIL%"
goto after_email
