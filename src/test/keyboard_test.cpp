#include <base/str.h>

#include <engine/client/keyboard.h>
#include <engine/keys.h>

#include <gtest/gtest.h>

TEST(KeyNameHumanReadable, Numpad)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_0), "Numpad 0");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_1), "Numpad 1");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_DIVIDE), "Numpad Divide");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_ENTER), "Numpad Enter");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_MULTIPLY), "Numpad Multiply");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_PLUS), "Numpad Plus");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_KP_PERIOD), "Numpad Period");
}

TEST(KeyNameHumanReadable, Mouse)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_1), "Left mouse");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_2), "Right mouse");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_3), "Middle mouse");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_4), "Mouse 4");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_5), "Mouse 5");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_6), "Mouse 6");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_7), "Mouse 7");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_8), "Mouse 8");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_9), "Mouse 9");
}

TEST(KeyNameHumanReadable, MouseWheel)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_WHEEL_UP), "Mouse wheel Up");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_WHEEL_DOWN), "Mouse wheel Down");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_WHEEL_LEFT), "Mouse wheel Left");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_MOUSE_WHEEL_RIGHT), "Mouse wheel Right");
}

TEST(KeyNameHumanReadable, ModifierKeys)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_LALT), "Left Alt");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_LCTRL), "Left Ctrl");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_LGUI), "Left Gui");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_LSHIFT), "Left Shift");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_RALT), "Right Alt");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_RCTRL), "Right Ctrl");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_RGUI), "Right Gui");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_RSHIFT), "Right Shift");
}

TEST(KeyNameHumanReadable, RegularKeys)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_A), "A");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_SPACE), "Space");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_RETURN), "Return");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_ESCAPE), "Escape");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_F1), "F1");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_RIGHT), "Right");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_LEFT), "Left");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_UP), "Up");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_DOWN), "Down");
}

TEST(KeyNameHumanReadable, UnknownKey)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_UNKNOWN), "");
}

TEST(KeyNameHumanReadable, JoystickButtons)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOYSTICK_BUTTON_0), "Joystick Button 0");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOYSTICK_BUTTON_1), "Joystick Button 1");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOYSTICK_BUTTON_23), "Joystick Button 23");
}

TEST(KeyNameHumanReadable, JoystickDpad)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_HAT0_UP), "D-Pad 0 Up");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_HAT0_DOWN), "D-Pad 0 Down");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_HAT0_LEFT), "D-Pad 0 Left");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_HAT0_RIGHT), "D-Pad 0 Right");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_HAT1_UP), "D-Pad 1 Up");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_HAT1_RIGHT), "D-Pad 1 Right");
}

TEST(KeyNameHumanReadable, JoystickAxis)
{
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_AXIS_0_LEFT), "Stick 0 Left");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_AXIS_0_RIGHT), "Stick 0 Right");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_AXIS_11_LEFT), "Stick 11 Left");
	EXPECT_STREQ(KeyNameHumanReadable(KEY_JOY_AXIS_11_RIGHT), "Stick 11 Right");
}

TEST(KeyNameHumanReadable, UnnamedKeys)
{
	EXPECT_STREQ(KeyNameHumanReadable(1), "Button 1");
	EXPECT_STREQ(KeyNameHumanReadable(2), "Button 2");
	EXPECT_STREQ(KeyNameHumanReadable(3), "Button 3");
	EXPECT_STREQ(KeyNameHumanReadable(130), "Button 130");
	EXPECT_STREQ(KeyNameHumanReadable(165), "Button 165");
	EXPECT_STREQ(KeyNameHumanReadable(222), "Button 222");
	EXPECT_STREQ(KeyNameHumanReadable(232), "Button 232");
	EXPECT_STREQ(KeyNameHumanReadable(360), "Button 360");
	EXPECT_STREQ(KeyNameHumanReadable(511), "Button 511");
}

TEST(KeyNameHumanReadable, AllKeys)
{
	for(int Key = KEY_FIRST; Key < KEY_LAST; ++Key)
	{
		const char *pKeyName = KeyNameHumanReadable(Key);

		if(Key == KEY_UNKNOWN)
			EXPECT_STREQ(pKeyName, "");
		else
			EXPECT_NE(pKeyName[0], '\0') << "Key " << Key << " (\"" << KeyName(Key) << "\") produced an empty name";
	}
}
