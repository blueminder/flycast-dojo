@ECHO OFF

IF EXIST flycast_previous (
	IF EXIST "flycast\mappings" (
		ECHO Copying mappings folder to active install
		XCOPY /I /E /Q /R /Y "flycast\mappings" "flycast_previous\mappings"
	)

	IF EXIST "flycast\ROMs" (
		ECHO Copying ROMs folder to active install
		XCOPY /I /E /Q /R /Y "flycast\ROMs" "flycast_previous\ROMs"
	)

	IF EXIST "flycast\data\naomi.zip" (
		IF NOT EXIST "flycast_previous\ROMs\naomi.zip" (
			ECHO Copying naomi.zip to the ROMs folder
			XCOPY "flycast\data\naomi.zip" "flycast_previous\ROMs\" /Y >NUL
		)
	)

	IF EXIST "flycast\data\awbios.zip" (
		IF NOT EXIST "flycast_previous\ROMs\awbios.zip" (
			ECHO Copying awbios.zip to the ROMs folder
			XCOPY "flycast\data\awbios.zip" "flycast_previous\ROMs\" /Y >NUL
		)
	)

	IF EXIST "flycast\data\naomi2.zip" (
		IF NOT EXIST "flycast_previous\ROMs\naomi2.zip" (
			ECHO Copying naomi2.zip to the ROMs folder
			XCOPY "flycast\data\naomi2.zip" "flycast_previous\ROMs\" /Y >NUL
		)
	)

	ECHO Switching Flycast Dojo Version...
	REN flycast flycast_tmp
	REN flycast_previous flycast
	REN flycast_tmp flycast_previous
	ECHO Success!
	GOTO WAIT
)

SET index=1

SETLOCAL ENABLEDELAYEDEXPANSION
FOR %%f IN (flycast-dojo-*.zip) DO (
   SET file!index!=%%f
   ECHO !index!. %%f
   SET /A index=!index!+1
)

SETLOCAL DISABLEDELAYEDEXPANSION

SET /P selection="Select Version: "

SET file%selection% >nul 2>&1

IF ERRORLEVEL 1 (
   ECHO invalid number selected
   EXIT /B 1
)

CALL :RESOLVE %%file%selection%%%

ECHO Renaming existing flycast folder
REN flycast flycast_previous

ECHO Extracting %file_name%
POWERSHELL Expand-Archive %file_name% -DestinationPath .

SET noext=%file_name:~0,-4%

ECHO Replacing flycast folder
REN %noext% flycast

IF EXIST "flycast_previous\mappings" (
	ECHO Copying mappings folder
	XCOPY /I /E /Q /R /Y "flycast_previous\mappings" "flycast\mappings"
)

IF EXIST "flycast_previous\ROMs" (
	ECHO Copying ROMs folder
	XCOPY /I /E /Q /R /Y "flycast_previous\ROMs" "flycast\ROMs"
)

IF EXIST "flycast_previous\data\naomi.zip" (
	ECHO Copying naomi.zip to the ROMs folder
	XCOPY "flycast_previous\data\naomi.zip" "flycast\ROMs\" /Y >NUL
)

IF EXIST "flycast_previous\data\awbios.zip" (
	ECHO Copying awbios.zip to the ROMs folder
	XCOPY "flycast_previous\data\awbios.zip" "flycast\ROMs\" /Y >NUL
)

IF EXIST "flycast_previous\data\naomi2.zip" (
	ECHO Copying naomi2.zip to the ROMs folder
	XCOPY "flycast_previous\data\naomi2.zip" "flycast\ROMs\" /Y >NUL
)

ECHO Success!
GOTO :WAIT

:LOCKED
ECHO Close any other programs that are accessing the folder and try again.

:WAIT
PAUSE
GOTO :EOF

:RESOLVE
SET file_name=%1
GOTO :EOF
