#include <engine/engine.h>
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

		auto StartTime = time_get_nanoseconds();
		std::shared_ptr<CHttpRequest> pGet = HttpGet("https://ger10.ddnet.org/");
		pGet->Timeout(CTimeout{10000, 0, 500, 10});
		pGet->IpResolve(IPRESOLVE::V4);

		Http()->Run(pGet);

		auto Time = std::chrono::duration_cast<std::chrono::milliseconds>(time_get_nanoseconds() - StartTime);
		if(pGet->State() != EHttpState::DONE)
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
			unsigned char *ppResult[128];
			size_t pResultLength[128];
			pGet->Result(ppResult, pResultLength);
			dbg_msg("verify", "Verified client! Took %d ms", (int)Time.count());
			verified = true;
		}
		return;
	}
}
