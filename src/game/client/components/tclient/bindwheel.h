#ifndef GAME_CLIENT_COMPONENTS_BINDWHEEL_H
#define GAME_CLIENT_COMPONENTS_BINDWHEEL_H
#include <game/client/component.h>
class IConfigManager;

enum
{
	BINDWHEEL_MAX_NAME = 64,
	BINDWHEEL_MAX_CMD = 1024,
	BINDWHEEL_MAX_BINDS = 64
};

class CBindWheel : public CComponent
{
	void DrawCircle(float x, float y, float r, int Segments);

	bool m_WasActive;
	bool m_Active;

	vec2 m_SelectorMouse;
	int m_SelectedBind;

	static void ConOpenBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConAddBindwheelLegacy(IConsole::IResult *pResult, void *pUserData);
	static void ConAddBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveBindwheel(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveAllBindwheelBinds(IConsole::IResult *pResult, void *pUserData);
	static void ConBindwheelExecuteHover(IConsole::IResult *pResult, void *pUserData);

	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

public:
	class CBind
	{
	public:
		char m_aName[BINDWHEEL_MAX_NAME];
		char m_aCommand[BINDWHEEL_MAX_CMD];

		bool operator==(const CBind &Other) const
		{
			return str_comp(m_aName, Other.m_aName) == 0 && str_comp(m_aCommand, Other.m_aCommand) == 0;
		}
	};

	std::vector<CBind> m_vBinds;

	CBindWheel();
	virtual int Sizeof() const override { return sizeof(*this); }

	virtual void OnReset() override;
	virtual void OnRender() override;
	virtual void OnConsoleInit() override;
	virtual void OnRelease() override;
	virtual bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	void AddBind(const char *Name, const char *Command);
	void RemoveBind(const char *Name, const char *Command);
	void RemoveBind(int Index);
	void RemoveAllBinds();

	void ExecuteHoveredBind();
	void ExecuteBind(int Bind);
};

#endif
