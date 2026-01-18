@echo off
REM Simple single file C/C++ compilation script.
REM Pass in the file to compile as the argument 
REM or pass no arguments to compile the default: `test.c`


set "vcvars_path=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if "%~1" == "" (
    set "InputFile=test.c"
) else (
    set "InputFile=%1"
)

set "BuildFolder=build"
set "Debug=1"
REM set "Sanitizer=/fsanitize=address"

set "CFlags=/nologo /W4 /wd4200 /wd4146 /wd4127 /I../src"
set "LinkFlags=/link /INCREMENTAL:NO"

if "%Debug%"=="1" (
    set "CFlags=%CFlags% %Sanitizer% /Zi /DMIGI_DEBUG_LOGS"
) else (
    set "CFlags=%CFlags% /O2"
)

if not exist "%BuildFolder%" (
    mkdir "%BuildFolder%"
)

pushd "%BuildFolder%"

where cl >nul 2>nul
if errorlevel 1 (
    if not exist "%vcvars_path%" (
        echo vcvars not found in path.
        exit /b 1
    )
    echo Preparing environment...
    call "%vcvars_path%" >nul 2>nul
)


for %%A in ("../%InputFile%") do set "InputFileAbsolute=%%~fA"
echo cl %CFlags% %InputFileAbsolute% %LinkFlags%
cl %CFlags% %InputFileAbsolute% %LinkFlags%
set "result=%errorlevel%"

popd

exit /b %comp_result%
