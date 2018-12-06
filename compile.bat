@echo off
title chillerbot-ux  compiler
cd build

:compiler
	cmake --build .
	move Debug\DDNet.exe C:\Users\%USERNAME%\Desktop\teeworlds_clients\chillerbot-ux\chillerbot-ux.exe
	echo press any key to compile agian
	pause >NUL
goto compiler