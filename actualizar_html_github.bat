@echo off
setlocal EnableExtensions

REM Publica el contenido de esta carpeta al repo publico AeroDeck-HTML.
REM Incluye todas las subcarpetas (Attitud_Control, futuras, etc.).
set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"

set "TARGET_URL=https://github.com/Claudio-Arz/AeroDeck-HTML.git"
set "TARGET_BRANCH=main"
set "WORKDIR=%TEMP%\AeroDeck-HTML_sync"
set "TARGET=%WORKDIR%\AeroDeck-HTML"

echo =============================================
echo Publicar en repo publico - Instrumentos_Digitales
echo Source: %SRC%
echo Target: %TARGET_URL%
echo =============================================

if not exist "%WORKDIR%" mkdir "%WORKDIR%"

if not exist "%TARGET%\.git" (
    echo Clonando repo publico por primera vez...
    git clone "%TARGET_URL%" "%TARGET%"
    if errorlevel 1 goto :error
)

pushd "%TARGET%"
git fetch origin
if errorlevel 1 goto :error_pop

git checkout %TARGET_BRANCH%
if errorlevel 1 goto :error_pop

git pull --rebase origin %TARGET_BRANCH%
if errorlevel 1 goto :error_pop
popd

echo.
echo Sincronizando archivos...
robocopy "%SRC%" "%TARGET%" /MIR /XD ".git" /XF "*.bat" /R:2 /W:1 /NFL /NDL /NJH /NJS /NP
if errorlevel 8 goto :error

pushd "%TARGET%"
git status -sb
echo.

git add -A
git diff --cached --quiet
if not errorlevel 1 (
    echo No hay cambios para publicar.
    popd
    echo.
    echo URL publica: https://claudio-arz.github.io/AeroDeck-HTML/
    pause
    exit /b 0
)

set /p "MSG=Mensaje de commit para AeroDeck-HTML (vacio = cancelar): "
if "%MSG%"=="" (
    echo Operacion cancelada por el usuario.
    popd
    pause
    exit /b 0
)

set /p "GO=Publicar en GitHub con ese mensaje? (S/N): "
if /I not "%GO%"=="S" (
    echo Operacion cancelada por el usuario.
    popd
    pause
    exit /b 0
)

git commit -m "%MSG%"
if errorlevel 1 goto :error_pop

git push origin %TARGET_BRANCH%
if errorlevel 1 goto :error_pop

echo.
echo OK: Publicado en GitHub Pages.
echo URL publica: https://claudio-arz.github.io/AeroDeck-HTML/
popd
pause
exit /b 0

:error_pop
popd

:error
echo.
echo ERROR: Revisar mensajes de Git arriba.
pause
