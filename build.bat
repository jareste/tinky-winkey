@echo off

echo Building in tinky directory...
cd tinky
svc.exe delete
nmake re
if %errorlevel% neq 0 (
    echo Failed to build tinky.
    exit /b %errorlevel%
)
cd ..

echo Building in winkey directory...
cd winkey
nmake re
if %errorlevel% neq 0 (
    echo Failed to build winkey.
    exit /b %errorlevel%
)
cd ..

cd tinky
echo Installing tinky service...
svc.exe install
timeout /t 1 /nobreak
echo Starting tinky service...
svc.exe start

echo Build completed for both tinky and winkey.
