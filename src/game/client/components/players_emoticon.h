#ifndef GAME_CLIENT_COMPONENTS_PLAYERS_EMOTICON_H
#define GAME_CLIENT_COMPONENTS_PLAYERS_EMOTICON_H
#include <game/client/component.h>

struct CNetObj_Character;

class CPlayersEmoticon : public CComponent
{
	void RenderEmoticon(
		const CNetObj_Character *pPlayerChar,
		int ClientID);

	int m_EmoteQuadContainerIndex;

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif
