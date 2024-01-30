#include <engine/shared/config.h>

#include "../entities/character.h"
#include "../gamecontext.h"
#include "../gamecontroller.h"
#include "../gamemodes/DDRace.h"
#include "../gamemodes/gctf.h"
#include "../gamemodes/ictf.h"
#include "../gamemodes/mod.h"
#include "../player.h"

#include "strhelpers.h"

void IGameController::OnEndMatchInsta()
{
	dbg_msg("ddnet-insta", "match end");
}

void IGameController::GetRoundEndStatsStr(char *pBuf, size_t Size)
{
}

void IGameController::PublishRoundEndStatsStr(const char *pStr)
{
}
