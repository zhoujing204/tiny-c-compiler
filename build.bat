@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /W3 /Od /Zi /Fe:build\tcc.exe ^
    src\tcc.c ^
    src\lex.c ^
    src\parse.c ^
    src\sym.c ^
    src\gen.c ^
    src\x86_64-gen.c ^
    src\pe.c ^
    src\section.c ^
    src\utils.c ^
    /I src ^
    /link /DEBUG
del *.obj 2>nul
echo.
echo Build complete!
