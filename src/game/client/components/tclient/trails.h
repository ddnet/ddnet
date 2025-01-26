#ifndef GAME_CLIENT_COMPONENTS_TRAILS_H
#define GAME_CLIENT_COMPONENTS_TRAILS_H
#include <game/client/component.h>
class STrailPart
{
public:
	vec2 Pos = vec2(0, 0);
	vec2 UnmovedPos = vec2(0, 0);
	ColorRGBA Col = {};
	float Width = 0.0f;
	vec2 Normal = vec2(0, 0);
	vec2 Top = vec2(0, 0);
	vec2 Bot = vec2(0, 0);
	bool Flip = false;
	float Progress = 1.0f;
	int Tick = -1;

	bool operator==(const STrailPart &Other) const
	{
		return Pos == Other.Pos;
	}
};

class CTrails : public CComponent
{
public:
	CTrails() = default;
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;

	enum COLORMODES
	{
		COLORMODE_SOLID = 1,
		COLORMODE_TEE,
		COLORMODE_RAINBOW,
		COLORMODE_SPEED,
	};

private:
	class CInfo
	{
	public:
		vec2 m_Pos;
		int m_Tick;
	};
	CInfo m_History[MAX_CLIENTS][200];
	bool m_HistoryValid[MAX_CLIENTS] = {};

	void ClearAllHistory();
	void ClearHistory(int ClientId);
	bool ShouldPredictPlayer(int ClientId);
};

#endif
