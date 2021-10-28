# pylint: skip-file
# generate keys.h file
f = file("src/engine/keys.h", "w")

keynames = []
for i in range(0, 512):
	keynames += ["&%d"%i]

print >>f, "#ifndef ENGINE_KEYS_H"
print >>f, "#define ENGINE_KEYS_H"

print >>f, '/* AUTO GENERATED! DO NOT EDIT MANUALLY! */'
print >>f, 'namespace KEY'
print >>f, "{"
print >>f, "enum"
print >>f, "{"

print >>f, "\tFIRST = 0,"

highestid = 0
for line in open("scripts/SDL_scancode.h"):
	l = line.strip().split("=")
	if len(l) == 2 and "SDL_SCANCODE_" in line:
		key = l[0].strip().replace("SDL_SCANCODE_", "")
		value = int(l[1].split(",")[0].strip())
		if key[0:2] == "/*":
			continue
		print >>f, "\t%s = %d,"%(key, value)

		keynames[value] = key.replace("", "").lower()

		if value > highestid:
			highestid =value

print >>f, "\tMOUSE_1 = %d,"%(highestid+1); keynames[highestid+1] = "mouse1"
print >>f, "\tMOUSE_2 = %d,"%(highestid+2); keynames[highestid+2] = "mouse2"
print >>f, "\tMOUSE_3 = %d,"%(highestid+3); keynames[highestid+3] = "mouse3"
print >>f, "\tMOUSE_4 = %d,"%(highestid+4); keynames[highestid+4] = "mouse4"
print >>f, "\tMOUSE_5 = %d,"%(highestid+5); keynames[highestid+5] = "mouse5"
print >>f, "\tMOUSE_6 = %d,"%(highestid+6); keynames[highestid+6] = "mouse6"
print >>f, "\tMOUSE_7 = %d,"%(highestid+7); keynames[highestid+7] = "mouse7"
print >>f, "\tMOUSE_8 = %d,"%(highestid+8); keynames[highestid+8] = "mouse8"
print >>f, "\tMOUSE_9 = %d,"%(highestid+9); keynames[highestid+9] = "mouse9"
print >>f, "\tMOUSE_WHEEL_UP = %d,"%(highestid+10); keynames[highestid+10] = "mousewheelup"
print >>f, "\tMOUSE_WHEEL_DOWN = %d,"%(highestid+11); keynames[highestid+11] = "mousewheeldown"
print >>f, "\tMOUSE_WHEEL_LEFT = %d,"%(highestid+12); keynames[highestid+12] = "mousewheelleft"
print >>f, "\tMOUSE_WHEEL_RIGHT = %d,"%(highestid+13); keynames[highestid+13] = "mousewheelright"
print >>f, "\tLAST = 512,"

print >>f, "};"
print >>f, ""
print >>f, "#endif"

# generate keynames.c file
f = file("src/engine/client/keynames.h", "w")
print >>f, '/* AUTO GENERATED! DO NOT EDIT MANUALLY! */'
print >>f, ''
print >>f, '#ifndef KEYS_INCLUDE'
print >>f, '#error do not include this header!'
print >>f, '#endif'
print >>f, ''
print >>f, "#include <string.h>"
print >>f, ""
print >>f, "const char g_aaKeyStrings[512][20] ="
print >>f, "{"
for n in keynames:
	print >>f, '\t"%s",'%n

print >>f, "};"
print >>f, "}"
print >>f, ""

f.close()
