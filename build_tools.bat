@ECHO OFF
SetLocal EnableDelayedExpansion

FOR %%f in (tools/*.c) do (
	ECHO "Building %%~nf..."
	clang "tools/%%f" -o "./tools/bin/%%~nf.exe" -D_CRT_SECURE_NO_WARNINGS
)

REM -Wall -Werror
REM SET compilerFlags=-g
REM SET includeFlags=-Isrc -I../engine/src/
REM SET linkerFlags=-L../bin/ -lengine.lib
REM SET defines=-D_DEBUG -DZIMPORT

REM clang++ %cppFiles% %compilerFlags% -o ../bin/%assembly%.exe %defines% %includeFlags% %linkerFlags%
