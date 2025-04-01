@ECHO OFF

:: USAGE EXAMPLE: tool obj_flip_uv res/backpack/backpack.obj res/backpack/flipped.obj

SetLocal EnableDelayedExpansion

set exe=%1

set counter=0

for %%A in (%*) do (
	set /a counter+=1
	if !counter! gtr 1 (
		set "args=!args! %%A"
	)
)

"tools/bin/%exe%.exe"%args%
