#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/render.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>

#include "verify.h"

static bool verified = false;
static int tries = 0;
void CVerify::OnRender()
{
	if(g_Config.m_ClAutoVerify)
	{
		// check if we are already verified
		if(verified)
			return;

		CTimeout Timeout{10000, 0, 8000, 10};
		auto StartTime = time_get_nanoseconds();
		CHttpRequest *pGet = HttpGet("https://ger10.ddnet.tw/").release();
		pGet->Timeout(Timeout);
		IEngine::RunJobBlocking(pGet);
		auto Time = std::chrono::duration_cast<std::chrono::milliseconds>(time_get_nanoseconds() - StartTime);
		if(pGet->State() != HTTP_DONE)
		{
			dbg_msg("verify", "Failed to verify client");
			tries++;
			if(tries >= 3)
			{
				dbg_msg("verify", "Failed to verify client 3 times, disabling auto verify");
				verified = true;
			}
		}
		else
		{
			unsigned char *cChar[128];
			size_t cSize[128];
			pGet->Result(cChar, cSize);
			dbg_msg("verify", "Verified client! Took %d ms", (int)Time.count());
			verified = true;
		}
		return;
	}
}