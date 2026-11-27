@echo off
rem =====================================================================
rem  clean_old_files.bat - Xoa file trung lap / khong con dung
rem    - File cau truc cu o goc main\ (da chuyen vao config\ hal\ gfx\ app\ net\ tests\)
rem    - Font cu khong con dung (chi giu DejaVu Dam / Mong)
rem    - Ban sao luu va file zip tam
rem  Chay lai nhieu lan cung khong sao. Goi tu dong boi git_setup.bat.
rem =====================================================================
setlocal
cd /d "%~dp0.."
echo Don file trung lap / khong can thiet trong %CD% ...

rem --- Cau truc cu o goc main\ ---
for %%F in (app_config.h display.cpp display.h display_task.cpp display_task.h settings.cpp settings.h tests.cpp tests.h text_renderer.cpp text_renderer.h web_server.cpp web_server.h wifi_manager.cpp wifi_manager.h main.cpp.bak) do (
    if exist "main\%%F" del /q "main\%%F" && echo   xoa main\%%F
)
if exist "main\fonts" rmdir /s /q "main\fonts" && echo   xoa main\fonts\
if exist "main\web"   rmdir /s /q "main\web"   && echo   xoa main\web\

rem --- Font khong con dung ---
if exist "main\gfx\fonts\font_narrow.h" del /q "main\gfx\fonts\font_narrow.h" && echo   xoa main\gfx\fonts\font_narrow.h
for %%F in (Jersey10-Regular.ttf PressStart2P-Regular.ttf Silkscreen-Bold.ttf Silkscreen-Regular.ttf Tiny5-Regular.ttf VT323-Regular.ttf classic5x7.txt spleen-5x8.bdf spleen-6x12.bdf spleen-8x16.bdf tomthumb.h DejaVuSansCondensed-Bold.ttf) do (
    if exist "tools\gen_fonts\font_src\%%F" del /q "tools\gen_fonts\font_src\%%F" && echo   xoa tools\gen_fonts\font_src\%%F
)
if exist "tools\gen_fonts\__pycache__" rmdir /s /q "tools\gen_fonts\__pycache__"
if exist "tools\gen_fonts\preview.png" del /q "tools\gen_fonts\preview.png"

rem --- File tam ---
if exist "Claude outputs" rmdir /s /q "Claude outputs" && echo   xoa Claude outputs\

echo Xong.
if /i not "%~1"=="nopause" pause
exit /b 0
