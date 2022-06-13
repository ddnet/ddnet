# pylint: skip-file
# generate keys.h file
f = open("src/engine/keys.h", "w")

keynames = []
for i in range(0, 512):
	keynames += ["&%d"%i]

print("#ifndef ENGINE_KEYS_H", file=f)
print("#define ENGINE_KEYS_H", file=f)

# KEY_EXECUTE already exists on windows platforms
print("#if defined(CONF_FAMILY_WINDOWS)", file=f)
print("   #undef KEY_EXECUTE", file=f)
print("#endif", file=f)

print('/* AUTO GENERATED! DO NOT EDIT MANUALLY! */', file=f)
print("enum", file=f)
print("{", file=f)

print("\tKEY_FIRST = 0,", file=f)

highestid = 0
for line in open("scripts/SDL_scancode.h"):
	l = line.strip().split("=")
	if len(l) == 2 and "SDL_SCANCODE_" in line:
		key = l[0].strip().replace("SDL_SCANCODE_", "KEY_")
		value = int(l[1].split(",")[0].strip())
		if key[0:2] == "/*":
			continue
		print("\t%s = %d,"%(key, value), file=f)

		keynames[value] = key.replace("KEY_", "").lower()

		if value > highestid:
			highestid = value

highestid += 1
print("\tKEY_MOUSE_1 = %d,"%(highestid), file=f); keynames[highestid] = "mouse1"; highestid += 1
print("\tKEY_MOUSE_2 = %d,"%(highestid), file=f); keynames[highestid] = "mouse2"; highestid += 1
print("\tKEY_MOUSE_3 = %d,"%(highestid), file=f); keynames[highestid] = "mouse3"; highestid += 1
print("\tKEY_MOUSE_4 = %d,"%(highestid), file=f); keynames[highestid] = "mouse4"; highestid += 1
print("\tKEY_MOUSE_5 = %d,"%(highestid), file=f); keynames[highestid] = "mouse5"; highestid += 1
print("\tKEY_MOUSE_6 = %d,"%(highestid), file=f); keynames[highestid] = "mouse6"; highestid += 1
print("\tKEY_MOUSE_7 = %d,"%(highestid), file=f); keynames[highestid] = "mouse7"; highestid += 1
print("\tKEY_MOUSE_8 = %d,"%(highestid), file=f); keynames[highestid] = "mouse8"; highestid += 1
print("\tKEY_MOUSE_9 = %d,"%(highestid), file=f); keynames[highestid] = "mouse9"; highestid += 1
print("\tKEY_MOUSE_WHEEL_UP = %d,"%(highestid), file=f); keynames[highestid] = "mousewheelup"; highestid += 1
print("\tKEY_MOUSE_WHEEL_DOWN = %d,"%(highestid), file=f); keynames[highestid] = "mousewheeldown"; highestid += 1
print("\tKEY_MOUSE_WHEEL_LEFT = %d,"%(highestid), file=f); keynames[highestid] = "mousewheelleft"; highestid += 1
print("\tKEY_MOUSE_WHEEL_RIGHT = %d,"%(highestid), file=f); keynames[highestid] = "mousewheelright"; highestid += 1
print("\tKEY_LAST = 512,", file=f)

print("};", file=f)
print("", file=f)
print("#endif", file=f)

# generate keynames.c file
f = open("src/engine/client/keynames.h", "w")
print('/* AUTO GENERATED! DO NOT EDIT MANUALLY! */', file=f)
print('', file=f)
print('#ifndef KEYS_INCLUDE', file=f)
print('#error do not include this header!', file=f)
print('#endif', file=f)
print('', file=f)
print("#include <string.h>", file=f)
print("", file=f)
print("const char g_aaKeyStrings[512][20] = // NOLINT(misc-definitions-in-headers)", file=f)
print("{", file=f)
for n in keynames:
	print('\t"%s",'%n, file=f)

print("};", file=f)
print("", file=f)

f.close()
