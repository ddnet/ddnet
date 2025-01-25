#ifndef GAME_CLIENT_COMPONENTS_TRAILS_H
#define GAME_CLIENT_COMPONENTS_TRAILS_H
#include <game/client/component.h>

enum TRAIL_COLOR_MODES
{
	MODE_SOLID = 0,
	MODE_TEE,
	MODE_RAINBOW,
	MODE_SPEED,
	NUM_COLOR_MODES,
};

struct STrailPart
{
	vec2 Pos = vec2(0, 0);
	ColorRGBA Col = {};
	float Width = 0.0f;
	vec2 Normal = vec2(0, 0);
	vec2 Top = vec2(0, 0);
	vec2 Bot = vec2(0, 0);
	bool Flip = false;
	float Progress = 1.0f;

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

private:
	class CInfo
	{
	public:
		vec2 m_Pos, m_Vel;
	};
	CInfo m_PositionHistory[MAX_CLIENTS][200];
	int m_PositionTick[MAX_CLIENTS][200];
	// TODO add tick to CInfo
	bool m_HistoryValid[MAX_CLIENTS] = {};

	void ClearAllHistory();
	void ClearHistory(int ClientId);
	bool ShouldPredictPlayer(int ClientId);
};

#endif
