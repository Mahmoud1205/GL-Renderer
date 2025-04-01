@echo off

set config=%1

echo "[INFO]: Generating makefiles.."
premake5 gmake2
if %ERRORLEVEL% == 0 (
	echo "[INFO]: Finished generating makefiles!"

	echo "[INFO]: Building application.."
	pushd build
	make -j13 config=%config%_x86_64
	popd

	if %ERRORLEVEL% == 0 (
		echo "[INFO]: Finished building application!"	
	) else (
		echo "[ERROR]: Failed to build application.."
		exit /B 1
	)

	echo "[INFO]: Copying assets into bin folder.."
	REM /MIR to mirror the directory, all /N* are to silence the tool output
	robocopy "res" "bin/%config%/res" /MIR /NP /NFL /NDL /NJH /NJS /NS /NC
	xcopy "lib\Shared\*.dll" "bin\%config%\" /s /i /y
	xcopy "lib\%config%\*.dll" "bin\%config%\" /s /i /y
	echo "[INFO]: Completed full build process."
) else (
	echo "[ERROR]: Failed to generate makefiles.."
	exit /B 1
)
