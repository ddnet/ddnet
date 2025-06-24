#!/usr/bin/env python3

key_first = 0
key_last = 512

keynames = []
for i in range(key_first, key_last):
	keynames += [f"&{int(i)}"]

num_mouse_buttons = 9
num_joystick_buttons = 24
num_joystick_hats = 2
num_joystick_axes = 12

# generate keys.h file
with open("src/engine/keys.h", "w", encoding="utf-8") as f:
	print("/* AUTO GENERATED! DO NOT EDIT MANUALLY! See scripts/gen_keys.py */", file=f)
	print("", file=f)
	print("#ifndef ENGINE_KEYS_H", file=f)
	print("#define ENGINE_KEYS_H", file=f)
	print("", file=f)
	print("// KEY_EXECUTE already exists on Windows platforms", file=f)
	print("#if defined(CONF_FAMILY_WINDOWS)", file=f)
	print("#undef KEY_EXECUTE", file=f)
	print("#endif", file=f)
	print("", file=f)
	print("enum EKey", file=f)
	print("{", file=f)

	next_id = 0
	def print_key(enum_name, readable_name, numeric_id=-1):
		global next_id
		if numeric_id == -1:
			numeric_id = next_id
		if numeric_id >= key_last:
			raise RuntimeError("Too many keys, increase key_last")
		print(f"\t{enum_name} = {int(numeric_id)},", file=f)
		keynames[numeric_id] = readable_name
		next_id = max(next_id, numeric_id + 1)

	with open("scripts/SDL_scancode.h", 'r', encoding="utf-8") as scancodes_file:
		for line in scancodes_file:
			if "SDL_SCANCODE_" in line:
				l = line.strip().split("=")
				if len(l) == 2:
					key = l[0].strip().replace("SDL_SCANCODE_", "KEY_")
					value = int(l[1].split(",")[0].strip())
					if key[0:2] == "/*":
						continue
					print_key(key, key.replace("KEY_", "").lower(), value)

	print("", file=f)
	for i in range(1, num_mouse_buttons + 1):
		print_key(f"KEY_MOUSE_{i}", f"mouse{i}")
	print_key("KEY_MOUSE_WHEEL_UP", "mousewheelup")
	print_key("KEY_MOUSE_WHEEL_DOWN", "mousewheeldown")
	print_key("KEY_MOUSE_WHEEL_LEFT", "mousewheelleft")
	print_key("KEY_MOUSE_WHEEL_RIGHT", "mousewheelright")

	print("", file=f)
	for i in range(0, num_joystick_buttons):
		print_key(f"KEY_JOYSTICK_BUTTON_{i}", f"joystick{i}")

	print("", file=f)
	for i in range(0, num_joystick_hats):
		print_key(f"KEY_JOY_HAT{i}_UP", f"joy_hat{i}_up")
		print_key(f"KEY_JOY_HAT{i}_DOWN", f"joy_hat{i}_down")
		print_key(f"KEY_JOY_HAT{i}_LEFT", f"joy_hat{i}_left")
		print_key(f"KEY_JOY_HAT{i}_RIGHT", f"joy_hat{i}_right")

	print("", file=f)
	for i in range(0, num_joystick_axes):
		print_key(f"KEY_JOY_AXIS_{i}_LEFT", f"joy_axis{i}_left")
		print_key(f"KEY_JOY_AXIS_{i}_RIGHT", f"joy_axis{i}_right")

	print("", file=f)
	print(f"\tKEY_FIRST = {key_first},", file=f)
	print(f"\tKEY_LAST = {key_last},", file=f)

	print("", file=f)
	print(f"\tNUM_MOUSE_BUTTONS = {num_mouse_buttons},", file=f)
	print(f"\tNUM_JOYSTICK_BUTTONS = {num_joystick_buttons},", file=f)
	print("\tNUM_JOYSTICK_BUTTONS_PER_AXIS = KEY_JOY_AXIS_0_RIGHT - KEY_JOY_AXIS_0_LEFT + 1,", file=f)
	print(f"\tNUM_JOYSTICK_AXES = {num_joystick_axes},", file=f)
	print("\tNUM_JOYSTICK_BUTTONS_PER_HAT = KEY_JOY_HAT0_RIGHT - KEY_JOY_HAT0_UP + 1,", file=f)
	print(f"\tNUM_JOYSTICK_HATS = {num_joystick_hats},", file=f)

	print("};", file=f)
	print("", file=f)
	print("#endif", file=f)

# generate keynames.cpp file
with open("src/engine/client/keynames.cpp", "w", encoding="utf-8") as f:
	print("/* AUTO GENERATED! DO NOT EDIT MANUALLY! See scripts/gen_keys.py */", file=f)
	print("", file=f)
	print("#include \"keynames.h\"", file=f)
	print("", file=f)
	print("/**", file=f)
	print(" * Do not use directly! Use the @link IInput::KeyName @endlink function.", file=f)
	print(" */", file=f)
	print("extern const char g_aaKeyStrings[512][20] = {", file=f)
	for n in keynames:
		print(f"\t\"{n}\",", file=f)
	print("};", file=f)

# generate keynames.h file
with open("src/engine/client/keynames.h", "w", encoding="utf-8") as f:
	print("/* AUTO GENERATED! DO NOT EDIT MANUALLY! See scripts/gen_keys.py */", file=f)
	print("", file=f)
	print("#ifndef ENGINE_CLIENT_KEYNAMES_H", file=f)
	print("#define ENGINE_CLIENT_KEYNAMES_H", file=f)
	print("", file=f)
	print("/**", file=f)
	print(" * Do not use directly! Use the @link IInput::KeyName @endlink function.", file=f)
	print(" */", file=f)
	print("extern const char g_aaKeyStrings[512][20];", file=f)
	print("", file=f)
	print("#endif", file=f)
