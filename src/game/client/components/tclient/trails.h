#ifndef GAME_CLIENT_COMPONENTS_TRAILS_H
#define GAME_CLIENT_COMPONENTS_TRAILS_H
#include <game/client/component.h>

struct STrailPart
{
	vec2 Pos = vec2(0, 0);
	vec2 UnMovedPos = vec2(0, 0);
	ColorRGBA Col = {};
	float Alpha = 1.0f;
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
	CTrails();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	virtual void OnReset() override;

private:
	vec2 m_PositionHistory[MAX_CLIENTS][200];
	int m_PositionTick[MAX_CLIENTS][200];
	bool m_HistoryValid[MAX_CLIENTS] = {};

	void ClearAllHistory();
	void ClearHistory(int ClientId);
	bool ShouldPredictPlayer(int ClientId);
};

#endif
