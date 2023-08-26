#!/usr/bin/python
import os
import shutil

# make sure to have the x11 package xdotool installed on your system
def check_global_executable_exists(executable_name):
	return shutil.which(executable_name) is not None

def is_x11_session():
	return "DISPLAY" in os.environ and "WAYLAND_DISPLAY" not in os.environ

if is_x11_session() and check_global_executable_exists("xdotool"):
	print("Breakpoint hit, releasing mouse!")
	os.system("setxkbmap -option grab:break_actions")
	os.system("xdotool key XF86Ungrab")
