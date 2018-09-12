#!/usr/bin/env python2
import os,time,glob
def mtime(a):
    return os.path.getmtime(a) if os.path.exists(a) else 0
def d_mtime(a):
    return int(time.time()-mtime(a))
def is_i(a):
	return a.split("/")[-1][0]=='i'
expire=(10000,10000) #active connection cleanup interval,in-active(can be cleaned safely) conn cleanup interval (seconds)
def proc():
	for i in glob.glob("/dev/shm/tee/*/*"):
		if(d_mtime(i)>expire[is_i(i)]):
			os.unlink(i)
def proc2():
	for i in glob.glob("/dev/shm/tee/*"):
		if os.path.isdir(i) and (not os.listdir(i)):
			os.rmdir(i)
proc()
proc2()
