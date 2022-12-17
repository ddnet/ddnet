# pylint: skip-file
# generate keys.h file
f = open("src/engine/keys.h", "w")

keynames = []
for i in range(0, 512):
	keynames += [f"&{int(i)}"]

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
		print(f"\t{key} = {int(value)},", file=f)

		keynames[value] = key.replace("KEY_", "").lower()

		if value > highestid:
			highestid = value

highestid += 1
print("", file=f)
print(f"\tKEY_MOUSE_1 = {int(highestid)},", file=f); keynames[highestid] = "mouse1"; highestid += 1
print(f"\tKEY_MOUSE_2 = {int(highestid)},", file=f); keynames[highestid] = "mouse2"; highestid += 1
print(f"\tKEY_MOUSE_3 = {int(highestid)},", file=f); keynames[highestid] = "mouse3"; highestid += 1
print(f"\tKEY_MOUSE_4 = {int(highestid)},", file=f); keynames[highestid] = "mouse4"; highestid += 1
print(f"\tKEY_MOUSE_5 = {int(highestid)},", file=f); keynames[highestid] = "mouse5"; highestid += 1
print(f"\tKEY_MOUSE_6 = {int(highestid)},", file=f); keynames[highestid] = "mouse6"; highestid += 1
print(f"\tKEY_MOUSE_7 = {int(highestid)},", file=f); keynames[highestid] = "mouse7"; highestid += 1
print(f"\tKEY_MOUSE_8 = {int(highestid)},", file=f); keynames[highestid] = "mouse8"; highestid += 1
print(f"\tKEY_MOUSE_9 = {int(highestid)},", file=f); keynames[highestid] = "mouse9"; highestid += 1
print(f"\tKEY_MOUSE_WHEEL_UP = {int(highestid)},", file=f); keynames[highestid] = "mousewheelup"; highestid += 1
print(f"\tKEY_MOUSE_WHEEL_DOWN = {int(highestid)},", file=f); keynames[highestid] = "mousewheeldown"; highestid += 1
print(f"\tKEY_MOUSE_WHEEL_LEFT = {int(highestid)},", file=f); keynames[highestid] = "mousewheelleft"; highestid += 1
print(f"\tKEY_MOUSE_WHEEL_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "mousewheelright"; highestid += 1
print("", file=f)
print(f"\tKEY_JOYSTICK_BUTTON_0 = {int(highestid)},", file=f); keynames[highestid] = "joystick0"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_1 = {int(highestid)},", file=f); keynames[highestid] = "joystick1"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_2 = {int(highestid)},", file=f); keynames[highestid] = "joystick2"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_3 = {int(highestid)},", file=f); keynames[highestid] = "joystick3"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_4 = {int(highestid)},", file=f); keynames[highestid] = "joystick4"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_5 = {int(highestid)},", file=f); keynames[highestid] = "joystick5"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_6 = {int(highestid)},", file=f); keynames[highestid] = "joystick6"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_7 = {int(highestid)},", file=f); keynames[highestid] = "joystick7"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_8 = {int(highestid)},", file=f); keynames[highestid] = "joystick8"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_9 = {int(highestid)},", file=f); keynames[highestid] = "joystick9"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_10 = {int(highestid)},", file=f); keynames[highestid] = "joystick10"; highestid += 1
print(f"\tKEY_JOYSTICK_BUTTON_11 = {int(highestid)},", file=f); keynames[highestid] = "joystick11"; highestid += 1
print("", file=f)
print(f"\tKEY_JOY_HAT0_UP = {int(highestid)},", file=f); keynames[highestid] = "joy_hat0_up"; highestid += 1
print(f"\tKEY_JOY_HAT0_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_hat0_left"; highestid += 1
print(f"\tKEY_JOY_HAT0_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_hat0_right"; highestid += 1
print(f"\tKEY_JOY_HAT0_DOWN = {int(highestid)},", file=f); keynames[highestid] = "joy_hat0_down"; highestid += 1
print(f"\tKEY_JOY_HAT1_UP = {int(highestid)},", file=f); keynames[highestid] = "joy_hat1_up"; highestid += 1
print(f"\tKEY_JOY_HAT1_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_hat1_left"; highestid += 1
print(f"\tKEY_JOY_HAT1_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_hat1_right"; highestid += 1
print(f"\tKEY_JOY_HAT1_DOWN = {int(highestid)},", file=f); keynames[highestid] = "joy_hat1_down"; highestid += 1
print("", file=f)
print(f"\tKEY_JOY_AXIS_0_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis0_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_0_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis0_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_1_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis1_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_1_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis1_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_2_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis2_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_2_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis2_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_3_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis3_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_3_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis3_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_4_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis4_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_4_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis4_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_5_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis5_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_5_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis5_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_6_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis6_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_6_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis6_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_7_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis7_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_7_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis7_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_8_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis8_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_8_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis8_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_9_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis9_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_9_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis9_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_10_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis10_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_10_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis10_right"; highestid += 1
print(f"\tKEY_JOY_AXIS_11_LEFT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis11_left"; highestid += 1
print(f"\tKEY_JOY_AXIS_11_RIGHT = {int(highestid)},", file=f); keynames[highestid] = "joy_axis11_right"; highestid += 1
print("", file=f)
print("\tKEY_LAST = 512,", file=f)
print("", file=f)
print("\tNUM_JOYSTICK_BUTTONS = KEY_JOYSTICK_BUTTON_11 - KEY_JOYSTICK_BUTTON_0 + 1,", file=f)
print("\tNUM_JOYSTICK_AXES_BUTTONS = KEY_JOY_AXIS_11_RIGHT - KEY_JOY_AXIS_0_LEFT + 1,", file=f)
print("\tNUM_JOYSTICK_BUTTONS_PER_AXIS = KEY_JOY_AXIS_0_RIGHT - KEY_JOY_AXIS_0_LEFT + 1,", file=f)
print("\tNUM_JOYSTICK_AXES = NUM_JOYSTICK_AXES_BUTTONS / NUM_JOYSTICK_BUTTONS_PER_AXIS,", file=f)
print("\tNUM_JOYSTICK_HAT_BUTTONS = KEY_JOY_HAT1_DOWN - KEY_JOY_HAT0_UP + 1,", file=f)
print("\tNUM_JOYSTICK_BUTTONS_PER_HAT = KEY_JOY_HAT1_DOWN - KEY_JOY_HAT1_UP + 1,", file=f)
print("\tNUM_JOYSTICK_HATS = NUM_JOYSTICK_HAT_BUTTONS / NUM_JOYSTICK_BUTTONS_PER_HAT,", file=f)

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
print("const char g_aaKeyStrings[512][20] = // NOLINT(misc-definitions-in-headers)", file=f)
print("{", file=f)
for n in keynames:
	print(f'\t"{n}",', file=f)

print("};", file=f)
print("", file=f)

f.close()
