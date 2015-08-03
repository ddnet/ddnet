import urllib.request
import os
import zipfile
import sys
import re

FILES = {
    "freetype.dll":			("freetype.dll-{arch}",        "{builddir}"),
    "libcurl.dll":          ("libcurl.dll-{arch}",         "{builddir}"),
    "libeay32.dll":         ("libeay32.dll-{arch}",        "{builddir}"),
    "libgcc_s_sjlj-1.dll":  ("libgcc_s_sjlj-1.dll-{arch}", "{builddir}"),
    "libidn-11.dll":        ("libidn-11.dll-{arch}",       "{builddir}"),
    "libogg-0.dll":         ("libogg-0.dll-{arch}",        "{builddir}"),
    "libopus-0.dll":        ("libopus-0.dll-{arch}",       "{builddir}"),
    "libopusfile-0.dll":    ("libopusfile-0.dll-{arch}",   "{builddir}"),
    "SDL.dll":              ("SDL.dll-{arch}",             "{builddir}"),
    "ssleay32.dll":         ("ssleay32.dll-{arch}",        "{builddir}"),
    "zlib1.dll":            ("zlib1.dll-{arch}",           "{builddir}"),
    "curl":		            ("curl",                	   "other/curl/"),
    "freetype":		     	("freetype",            	   "other/freetype/"),
    "mysql":		    	("mysql",               	   "other/mysql/"),
    "opus":		    		("opus",                	   "other/opus/"),
    "sdl":		    		("sdl",                 	   "other/sdl/"),
}

def _downloadZip(url, filePath):
    # create full path, if it doesn't exists
    os.makedirs(filePath, exist_ok=True)
    # create zip-file at the temp fileposition
    temp_filePath = filePath + "temp.zip"
    urllib.request.urlretrieve(url, temp_filePath)
    # exctract full zip-file
    with zipfile.ZipFile(temp_filePath) as myzip:
        myzip.extractall(filePath)
    os.remove(temp_filePath)

def downloadAll(arch, conf, targets):
    download_url = "https://github.com/Zwelf/ddnet-downloads/raw/master/{}.zip".format
    builddir = "./"

    for target in targets:
        download_file, target_dir = FILES[target]
        _downloadZip(download_url(download_file.format(arch=arch)), target_dir.format(builddir=builddir))

def main():
    import argparse
    p = argparse.ArgumentParser(description="Download freetype and SDL library and header files for Windows.")
    p.add_argument("--arch", default="x86", choices=["x86", "x86_64"], help="Architecture for the downloaded libraries (Default: x86)")
    p.add_argument("--conf", default="debug", choices=["debug", "release"], help="Build type (Default: debug)")
    p.add_argument("targets", metavar="TARGET", nargs='+', choices=FILES, help='Target to download. Valid choices are "SDL.dll", "freetype.dll", "sdl" and "freetype"')
    args = p.parse_args()

    downloadAll(args.arch, args.conf, args.targets)

if __name__ == '__main__':
    main()
