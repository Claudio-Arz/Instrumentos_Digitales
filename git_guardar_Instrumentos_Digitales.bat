@echo off
setlocal

REM Usar la carpeta del script evita problemas de codificacion con tildes en rutas.
set "REPO=%~dp0"
if "%REPO:~-1%"=="\" set "REPO=%REPO:~0,-1%"

echo =============================================
echo Guardar y subir cambios - Instrumentos_Digitales
echo Repo: %REPO%
echo =============================================

git -C "%REPO%" rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
  echo ERROR: No se encontro el repositorio Git en:
  echo %REPO%
  pause
  exit /b 1
)

set /p "MSG=Mensaje de commit (vacio = cancelar): "
if "%MSG%"=="" (
  echo Operacion cancelada por el usuario.
  pause
  exit /b 0
)

pushd "%REPO%"
git status -sb
echo.

set /p "GO=Continuar con add/commit/pull/push? (S/N): "
if /I not "%GO%"=="S" (
  echo Operacion cancelada por el usuario.
  popd
  pause
  exit /b 0
)

git add -A
git diff --cached --quiet
if not errorlevel 1 (
  echo No hay cambios para commit.
  popd
  pause
  exit /b 0
)

git commit -m "%MSG%"
if errorlevel 1 goto :error

git pull --rebase
if errorlevel 1 goto :error

git push
if errorlevel 1 goto :error

echo.
echo OK: Cambios subidos correctamente.
popd
pause
exit /b 0

:error
echo.
echo ERROR: Revisar mensajes de Git arriba.
popd
pause
exit /b 1
