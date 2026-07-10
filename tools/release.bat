@echo off
setlocal EnableDelayedExpansion

REM ============================================================
REM release.bat
REM   Build the Distribute single-exe (all assets embedded) and
REM   publish a GitHub Release so the in-game auto-updater can
REM   detect / download / apply it.
REM
REM   usage:  tools\release.bat v1.0.1
REM           (or run with no arg and type the version)
REM
REM   Requires: Visual Studio (MSBuild) + GitHub CLI (gh auth login)
REM
REM   NOTE: To publish to a dedicated "releases only" repo, change
REM         REPO below AND keep GITHUB_OWNER/GITHUB_REPO in
REM         Src/Application/Updater/Updater.h in sync.
REM ============================================================

set "REPO=Hanoji-jp/Corelia"
set "ROOT=%~dp0.."
set "OUTDIR=%ROOT%\x64\Distribute"
set "NOTES=%~dp0release_notes_tmp.md"
set "STAGE=%~dp0_release_stage"

REM ---- version ----
set "VERSION=%~1"
if "%VERSION%"=="" set /p VERSION="Enter version (e.g. v1.0.1): "
if "%VERSION%"=="" (
    echo [ERROR] No version specified.
    pause & exit /b 1
)
REM Git tags cannot contain spaces. Reject them (use e.g. v0.0.1 or 0.0.1-snapshot).
if not "%VERSION%"=="%VERSION: =%" (
    echo [ERROR] Version must not contain spaces. Use e.g. v0.0.1 or 0.0.1-snapshot
    pause & exit /b 1
)
set "ZIP=%~dp0Corelia_%VERSION%.zip"

echo [INFO] Version : %VERSION%
echo [INFO] Repo    : %REPO%
echo [INFO] OutDir  : %OUTDIR%
echo.

REM ---- release notes (opens Notepad) ----
> "%NOTES%" echo ## %VERSION%
>>"%NOTES%" echo.
>>"%NOTES%" echo ### Changes
>>"%NOTES%" echo -
echo [INFO] Write release notes in Notepad, then save and close.
notepad "%NOTES%"

REM ---- write version.txt to project root (read by the updater) ----
> "%ROOT%\version.txt" echo %VERSION%
echo [INFO] version.txt = %VERSION%

REM ---- build Distribute (PreBuild packs Asset/ into the exe) ----
echo [INFO] Locating MSBuild...
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo [ERROR] MSBuild not found. Install Visual Studio with C++.
    pause & exit /b 1
)
echo [INFO] Building Distribute^|x64 ...
"%MSBUILD%" "%ROOT%\Project.vcxproj" /p:Configuration=Distribute /p:Platform=x64 /m /v:minimal
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause & exit /b 1
)
if not exist "%OUTDIR%\Project.exe" (
    echo [ERROR] Project.exe not found in %OUTDIR%.
    pause & exit /b 1
)

REM ---- stage the files that ship to players (exe + version.txt) ----
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /Y "%OUTDIR%\Project.exe" "%STAGE%\Project.exe" >NUL
copy /Y "%ROOT%\version.txt"   "%STAGE%\version.txt" >NUL

REM ---- zip (files at zip root so Expand-Archive drops them beside the exe) ----
echo [INFO] Creating zip...
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -Force"
rmdir /s /q "%STAGE%"
if not exist "%ZIP%" (
    echo [ERROR] Failed to create zip.
    pause & exit /b 1
)

REM ---- create the GitHub Release ----
echo [INFO] Creating GitHub Release...
gh release create "%VERSION%" "%ZIP%" --repo %REPO% --title "Release %VERSION%" --notes-file "%NOTES%"
if errorlevel 1 (
    echo [ERROR] gh release failed. Try: gh auth login
    del "%ZIP%"   2>NUL
    del "%NOTES%" 2>NUL
    pause & exit /b 1
)

echo.
echo [SUCCESS] Release %VERSION% published!
echo https://github.com/%REPO%/releases/tag/%VERSION%
del "%ZIP%"   2>NUL
del "%NOTES%" 2>NUL
pause
endlocal
