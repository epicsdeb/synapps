REM Tell medm where to look for .adl files.  Edit the following line, or comment it
REM out if the environment variable is already set
REM set EPICS_DISPLAY_PATH=C:\epics_adls
REM Note that we must use the Windows "start" command or medm won't find X11 dlls
start medm -x -macro "P=dxpXMAP:, D=dxp, M=mca" 16element_dxp.adl
REM Put Cygwin in the path so the EPICS application can find cygwin1.dll
PATH=c:\cygwin\bin
..\..\bin\cygwin-x86\dxpApp.exe st.cmd
