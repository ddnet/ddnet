
#ifndef GAME_CLIENT_COMPONENTS_BIND_WHEEL_H
#define GAME_CLIENT_COMPONENTS_BIND_WHEEL_H
#include <game/client/component.h>
class IConfigManager;

#define NUM_BINDWHEEL 8
#define MAX_BINDWHEEL_DESC 11
#define MAX_BINDWHEEL_CMD 128

class CBindWheel : public CComponent
{
    void DrawCircle(float x, float y, float r, int Segments);

	bool m_WasActive;
	bool m_Active;

	vec2 m_SelectorMouse;
	int m_SelectedBind;
	int m_SelectedEyeEmote;

	static void ConBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConBind(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	struct SClientBindWheel
	{
		char description[MAX_BINDWHEEL_DESC];
		char command[MAX_BINDWHEEL_CMD];
	};
	SClientBindWheel m_BindWheelList[NUM_BINDWHEEL];


	CBindWheel();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnConsoleInit() override;
	virtual void OnRelease() override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	static void ConchainBindwheel(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	
	void updateBinds(int Bindpos, char *Description, char *Command);
	void Binwheel(int Bind);

};

#endif
