# Contexto del proyecto LuisSystems

## Autor
Claudio Arzamendia — GitHub: https://github.com/Claudio-Arz

---

## Descripción general

Sistema de instrumentos digitales para cabina de avión (cockpit), desarrollado con ESP32 + HTML/CSS/JS.
El sistema tiene múltiples versiones en desarrollo paralelo. Solo se trabaja con **una versión a la vez**.

---

## Estructura de directorios en disco

```
C:\ESPLab\ZZZTmpTests\LuisSystems\
├── Instrumentos_Calibración_Versión002\AeroDeck_001\   ← repo: CockPit-V002
├── Instrumentos_Calibración_Versión003\AeroDeck_001\   ← repo: CockPit-V003
├── Instrumentos_Calibración_Versión004\AeroDeck_001\   ← repo: CockPit-V004
├── Instrumentos_Digitales\                             ← repo: Instrumentos_Digitales (este workspace)
└── _nested_git_backups\
```

---

## Repositorios en GitHub

| Directorio local | Repo GitHub | Rama activa |
|---|---|---|
| Instrumentos_Calibración_Versión003\AeroDeck_001 | https://github.com/Claudio-Arz/CockPit-V003 | `v003-main` |
| Instrumentos_Calibración_Versión004\AeroDeck_001 | https://github.com/Claudio-Arz/CockPit-V004 | `instrumentos-digitales`, `v004-main` |
| Instrumentos_Digitales | https://github.com/Claudio-Arz/Instrumentos_Digitales | `instrumentos-digitales` |
| (publicación en vivo) | https://github.com/Claudio-Arz/AeroDeck-HTML | `main` |

**AeroDeck-HTML** es el repo público que GitHub Pages sirve en vivo:
`https://claudio-arz.github.io/AeroDeck-HTML/`
Es sobreescrito (robocopy /MIR) por el sistema que esté activo en ese momento.

---

## Estructura de este workspace (Instrumentos_Digitales)

```
Instrumentos_Digitales\
├── .github\
│   └── copilot-instructions.md       ← este archivo
├── git_guardar_Instrumentos_Digitales.bat   ← guarda en repo Instrumentos_Digitales
├── actualizar_html_github.bat               ← publica TODO en AeroDeck-HTML
└── Attitud_Control\
    ├── Attitud_Control.ino                  ← código ESP32
    ├── AttitudeControl_Control.html
    ├── AttitudeControl_Instrumento.html
    └── Graphics\
        ├── AttCon_ball.png
        ├── AttCon_dial.png
        ├── AttCon_fondo.png
        └── AttCon_front.png
```

---

## Mecanismo de publicación (.bat)

Cada sistema tiene **dos .bat**:

### 1. `git_guardar_*.bat`
- Guarda el trabajo en el **repo fuente** del sistema (CockPit-V003, CockPit-V004, Instrumentos_Digitales)
- Flujo: `git add -A` → `git commit` → `git pull --rebase` → `git push`
- Usar primero (antes de actualizar_html)

### 2. `actualizar_html_github.bat`
- Publica los archivos en **AeroDeck-HTML** (lo que el sistema lee en vivo)
- Flujo: clona/actualiza AeroDeck-HTML en `%TEMP%\AeroDeck-HTML_sync\` → `robocopy /MIR` → commit → push
- En Instrumentos_Digitales: excluye `.bat`, incluye `.ino`, HTML, imágenes y subcarpetas futuras
- En Ver003 y Ver004: copia toda la carpeta `HTML\`

### Orden recomendado al trabajar
1. Ejecutar `git_guardar_*.bat` (backup del código fuente)
2. Ejecutar `actualizar_html_github.bat` (actualiza el sistema en vivo)

Solo trabaja **un sistema a la vez**. AeroDeck-HTML siempre refleja el último sistema que publicó.

---

## Notas importantes

- Ver004 funciona probado (Feb 2026). Ver003 e Instrumentos_Digitales están en desarrollo.
- Ver004 tiene una rama `instrumentos-digitales` en CockPit-V004 que ya NO debe contener la carpeta `Instrumentos_Digitales/` (fue eliminada en Mar 2026 — movida a su propio repo).
- Los `.bat` usan `%~dp0` para evitar problemas con rutas con tildes en Windows.
- Ver003 tenía un bat de publicación HTML desactualizado (reescrito en Mar 2026).
- AeroDeck-HTML no tiene ramas por versión — solo `main`. La separación por versión vive en los repos fuente.
