@echo off
REM Build monolith_query.exe — standalone SQLite query tool
REM Run from VS Developer Command Prompt, or let it find cl.exe via vswhere

where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo Building with cl.exe...
    cl /EHsc /std:c++17 /O2 /MT /I ThirdParty /I ..\MonolithProxy\ThirdParty /DSQLITE_ENABLE_FTS5 monolith_query.cpp ThirdParty\sqlite3.c /Fe:monolith_query.exe
    if %ERRORLEVEL% equ 0 goto :copy
    echo Build failed.
    exit /b 1
)

echo cl.exe not found in PATH.
echo Run from a Visual Studio Developer Command Prompt, or add cl.exe to PATH.
exit /b 1

:copy
if not exist ..\..\Binaries mkdir ..\..\Binaries
copy /Y monolith_query.exe ..\..\Binaries\monolith_query.exe
echo.
echo Built: Plugins\Monolith\Binaries\monolith_query.exe
