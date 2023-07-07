#ifndef GAME_CLIENT_COMPONENTS_COUNTDOWNS_H
#define GAME_CLIENT_COMPONENTS_COUNTDOWNS_H
#include <base/vmath.h>
#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

class CCountdowns : public CComponent
{
	void RenderBars();
	void RenderPlayerBars(int ClientID);
	void RenderFreezeBarAtPos(const vec2 *Pos, float Progress, float Alpha);
	void RenderNinjaBarAtPos(const vec2 *Pos, float Progress, float Alpha);
	void RenderBarAtPos(const vec2 *Pos, float Progress, float Alpha, IGraphics::CTextureHandle TextureFull, IGraphics::CTextureHandle TextureEmpty);

	bool IsPlayerInfoAvailable(int ClientID) const;

	void GenerateCountdownStars();
	void GenerateFreezeCountdownStars(int ClientID);
	void GenerateNinjaCountdownStars(int ClientID);

	enum
	{
		FREEZE_COUNTDOWN = 0,
		NINJA_COUNTDOWN = 1,
		NUMBER_OF_COUNTDOWN_TYPES = 2,
	};
	int m_aaLastGenerateTick[NUMBER_OF_COUNTDOWN_TYPES][MAX_CLIENTS];

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif
