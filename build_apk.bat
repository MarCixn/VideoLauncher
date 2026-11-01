@echo off
set JAVA_HOME=C:\Users\MarCixn\jdk21\jdk-21.0.4
set PATH=%JAVA_HOME%\bin;%PATH%
cd /d C:\Users\MarCixn\VideoLauncher
call C:\Users\MarCixn\gradle-8.5\bin\gradle.bat assembleDebug --no-daemon
