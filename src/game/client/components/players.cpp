/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "players.h"

#include <base/color.h>
#include <base/math.h>

#include <engine/client/enums.h>
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>
#include <generated/client_data7.h>
#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/components/controls.h>
#include <game/client/components/effects.h>
#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

static float CalculateHandAngle(vec2 Dir, float AngleOffset)
{
	const float Angle = angle(Dir);
	if(Dir.x < 0.0f)
	{
		return Angle - AngleOffset;
	}
	else
	{
		return Angle + AngleOffset;
	}
}

static vec2 CalculateHandPosition(vec2 CenterPos, vec2 Dir, vec2 PostRotOffset)
{
	vec2 DirY = vec2(-Dir.y, Dir.x);
	if(Dir.x < 0.0f)
	{
		DirY = -DirY;
	}
	return CenterPos + Dir + Dir * PostRotOffset.x + DirY * PostRotOffset.y;
}

void CPlayers::RenderHand(const CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset, float Alpha)
{
	const vec2 HandPos = CalculateHandPosition(CenterPos, Dir, PostRotOffset);
	const float HandAngle = CalculateHandAngle(Dir, AngleOffset);
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_HANDS).IsValid())
	{
		RenderHand7(pInfo, HandPos, HandAngle, Alpha);
	}
	else
	{
		RenderHand6(pInfo, HandPos, HandAngle, Alpha);
	}
}

void CPlayers::RenderHand7(const CTeeRenderInfo *pInfo, vec2 HandPos, float HandAngle, float Alpha)
{
	// in-game hand size is 15 when tee size is 64
	const float BaseSize = 15.0f * (pInfo->m_Size / 64.0f);
	IGraphics::CQuadItem QuadOutline(HandPos.x, HandPos.y, 2 * BaseSize, 2 * BaseSize);
	IGraphics::CQuadItem QuadHand = QuadOutline;

	Graphics()->TextureSet(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_HANDS));
	Graphics()->QuadsBegin();
	Graphics()->SetColor(pInfo->m_aSixup[g_Config.m_ClDummy].m_aColors[protocol7::SKINPART_HANDS].WithAlpha(Alpha));
	Graphics()->QuadsSetRotation(HandAngle);
	Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HAND_OUTLINE);
	Graphics()->QuadsDraw(&QuadOutline, 1);
	Graphics()->SelectSprite7(client_data7::SPRITE_TEE_HAND);
	Graphics()->QuadsDraw(&QuadHand, 1);
	Graphics()->QuadsEnd();
}

void CPlayers::RenderHand6(const CTeeRenderInfo *pInfo, vec2 HandPos, float HandAngle, float Alpha)
{
	const CSkin::CSkinTextures *pSkinTextures = pInfo->m_CustomColoredSkin ? &pInfo->m_ColorableRenderSkin : &pInfo->m_OriginalRenderSkin;

	Graphics()->SetColor(pInfo->m_ColorBody.WithAlpha(Alpha));
	Graphics()->QuadsSetRotation(HandAngle);
	Graphics()->TextureSet(pSkinTextures->m_HandsOutline);
	Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, NUM_WEAPONS * 2, HandPos.x, HandPos.y);
	Graphics()->TextureSet(pSkinTextures->m_Hands);
	Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, NUM_WEAPONS * 2 + 1, HandPos.x, HandPos.y);
}

float CPlayers::GetPlayerTargetAngle(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientId,
	float Intra)
{
	if(GameClient()->m_Snap.m_LocalClientId == ClientId && !GameClient()->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// calculate what would be sent to the server from our current input
		vec2 Direction = normalize(vec2((int)GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].x, (int)GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy].y));

		// fix direction if mouse is exactly in the center
		if(Direction == vec2(0.0f, 0.0f))
			Direction = vec2(1.0f, 0.0f);

		return angle(Direction);
	}

	// using unpredicted angle when rendering other players in-game
	if(ClientId >= 0)
		Intra = Client()->IntraGameTick(g_Config.m_ClDummy);

	if(ClientId >= 0 && GameClient()->m_Snap.m_aCharacters[ClientId].m_HasExtendedDisplayInfo)
	{
		const CNetObj_DDNetCharacter *pExtendedData = &GameClient()->m_Snap.m_aCharacters[ClientId].m_ExtendedData;
		const CNetObj_DDNetCharacter *pPrevExtendedData = GameClient()->m_Snap.m_aCharacters[ClientId].m_pPrevExtendedData;
		if(pPrevExtendedData)
		{
			float MixX = mix((float)pPrevExtendedData->m_TargetX, (float)pExtendedData->m_TargetX, Intra);
			float MixY = mix((float)pPrevExtendedData->m_TargetY, (float)pExtendedData->m_TargetY, Intra);
			return angle(vec2(MixX, MixY));
		}
		else
		{
			return angle(vec2(pExtendedData->m_TargetX, pExtendedData->m_TargetY));
		}
	}
	else
	{
		// If the player moves their weapon through top, then change
		// the end angle by 2*Pi, so that the mix function will use the
		// short path and not the long one.
		if(pPlayerChar->m_Angle > (256.0f * pi) && pPrevChar->m_Angle < 0)
		{
			return mix((float)pPrevChar->m_Angle, (float)(pPlayerChar->m_Angle - 256.0f * 2 * pi), Intra) / 256.0f;
		}
		else if(pPlayerChar->m_Angle < 0 && pPrevChar->m_Angle > (256.0f * pi))
		{
			return mix((float)pPrevChar->m_Angle, (float)(pPlayerChar->m_Angle + 256.0f * 2 * pi), Intra) / 256.0f;
		}
		else
		{
			return mix((float)pPrevChar->m_Angle, (float)pPlayerChar->m_Angle, Intra) / 256.0f;
		}
	}
}

void CPlayers::RenderHookCollLine(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientId)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	dbg_assert(in_range(ClientId, MAX_CLIENTS - 1), "invalid client id (%d)", ClientId);

	bool Local = GameClient()->m_Snap.m_LocalClientId == ClientId;

	float Intra = GameClient()->m_aClients[ClientId].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	float Angle = GetPlayerTargetAngle(&Prev, &Player, ClientId, Intra);

	vec2 Direction = direction(Angle);
	vec2 Position = GameClient()->m_aClients[ClientId].m_RenderPos;

	bool Aim = (Player.m_PlayerFlags & PLAYERFLAG_AIM);
	if(!Client()->ServerCapAnyPlayerFlag())
	{
		for(int i = 0; i < NUM_DUMMIES; i++)
		{
			if(ClientId == GameClient()->m_aLocalIds[i])
			{
				Aim = GameClient()->m_Controls.m_aShowHookColl[i];
				break;
			}
		}
	}

#if defined(CONF_VIDEORECORDER)
	if(IVideo::Current() && !g_Config.m_ClVideoShowHookCollOther && !Local)
		return;
#endif

	bool AlwaysRenderHookColl = GameClient()->m_GameInfo.m_AllowHookColl && (Local ? g_Config.m_ClShowHookCollOwn : g_Config.m_ClShowHookCollOther) == 2;
	bool RenderHookCollPlayer = Aim && (Local ? g_Config.m_ClShowHookCollOwn : g_Config.m_ClShowHookCollOther) > 0;
	if(Local && GameClient()->m_GameInfo.m_AllowHookColl && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		RenderHookCollPlayer = GameClient()->m_Controls.m_aShowHookColl[g_Config.m_ClDummy] && g_Config.m_ClShowHookCollOwn > 0;
	if(!AlwaysRenderHookColl && !RenderHookCollPlayer)
		return;

	static constexpr float HOOK_START_DISTANCE = CCharacterCore::PhysicalSize() * 1.5f;
	float HookLength = (float)GameClient()->m_aClients[ClientId].m_Predicted.m_Tuning.m_HookLength;
	float HookFireSpeed = (float)GameClient()->m_aClients[ClientId].m_Predicted.m_Tuning.m_HookFireSpeed;

	// janky physics
	if(HookLength < HOOK_START_DISTANCE || HookFireSpeed <= 0.0f)
		return;

	vec2 QuantizedDirection = Direction;
	vec2 StartOffset = Direction * HOOK_START_DISTANCE;
	vec2 BasePos = Position;
	vec2 LineStartPos = BasePos + StartOffset;
	vec2 SegmentStartPos = LineStartPos;

	ColorRGBA HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorNoColl));
	std::vector<IGraphics::CLineItem> vLineSegments;

	const int MaxHookTicks = 5 * Client()->GameTickSpeed(); // calculating above 5 seconds is very expensive and unlikely to happen

	// simulate the hook into the future
	int HookTick;
	bool HookEnteredTelehook = false;
	for(HookTick = 0; HookTick < MaxHookTicks; ++HookTick)
	{
		int Tele;
		vec2 HitPos;
		vec2 SegmentEndPos = SegmentStartPos + QuantizedDirection * HookFireSpeed;

		// check if a hook would enter retracting state in this tick
		if(distance(BasePos, SegmentEndPos) > HookLength)
		{
			// check if the retracting hook hits a player
			if(!HookEnteredTelehook)
			{
				vec2 RetractingHookEndPos = BasePos + normalize(SegmentEndPos - BasePos) * HookLength;
				if(GameClient()->IntersectCharacter(SegmentStartPos, RetractingHookEndPos, HitPos, ClientId) != -1)
				{
					HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl));
					vLineSegments.emplace_back(LineStartPos, HitPos);
					break;
				}
			}

			// the line is too long here, and the hook retracts, use old position
			vLineSegments.emplace_back(LineStartPos, SegmentStartPos);
			break;
		}

		// check for map collisions
		int Hit = Collision()->IntersectLineTeleHook(SegmentStartPos, SegmentEndPos, &HitPos, nullptr, &Tele);

		// check if we intersect a player
		if(GameClient()->IntersectCharacter(SegmentStartPos, HitPos, SegmentEndPos, ClientId) != -1)
		{
			HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl));
			vLineSegments.emplace_back(LineStartPos, SegmentEndPos);
			break;
		}

		// we hit nothing, continue calculating segments
		if(!Hit)
		{
			SegmentStartPos = SegmentEndPos;
			SegmentStartPos.x = round_to_int(SegmentStartPos.x);
			SegmentStartPos.y = round_to_int(SegmentStartPos.y);

			// direction is always the same after the first tick quantization
			if(HookTick == 0)
			{
				QuantizedDirection.x = round_to_int(QuantizedDirection.x * 256.0f) / 256.0f;
				QuantizedDirection.y = round_to_int(QuantizedDirection.y * 256.0f) / 256.0f;
			}
			continue;
		}

		// we hit a solid / hook stopper
		if(Hit != TILE_TELEINHOOK)
		{
			if(Hit != TILE_NOHOOK)
				HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl));
			vLineSegments.emplace_back(LineStartPos, HitPos);
			break;
		}

		// we are hitting TILE_TELEINHOOK
		vLineSegments.emplace_back(LineStartPos, HitPos);
		HookEnteredTelehook = true;

		// check tele outs
		const std::vector<vec2> &vTeleOuts = Collision()->TeleOuts(Tele - 1);
		if(vTeleOuts.empty())
		{
			// the hook gets stuck, this is a feature or a bug
			HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl));
			break;
		}
		else if(vTeleOuts.size() > 1)
		{
			// we don't know which teleout the hook takes, just invert the color
			HookCollColor = color_invert(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)));
			break;
		}

		// go through one teleout, update positions and continue
		BasePos = vTeleOuts[0];
		LineStartPos = BasePos; // make the line start in the teleporter to prevent a gap
		SegmentStartPos = BasePos + Direction * HOOK_START_DISTANCE;
		SegmentStartPos.x = round_to_int(SegmentStartPos.x);
		SegmentStartPos.y = round_to_int(SegmentStartPos.y);

		// direction is always the same after the first tick quantization
		if(HookTick == 0)
		{
			QuantizedDirection.x = round_to_int(QuantizedDirection.x * 256.0f) / 256.0f;
			QuantizedDirection.y = round_to_int(QuantizedDirection.y * 256.0f) / 256.0f;
		}
	}

	// The hook line is too expensive to calculate and didn't hit anything before, just set a straight line
	if(HookTick >= MaxHookTicks && vLineSegments.empty())
	{
		// we simply don't know if we hit anything or not
		HookCollColor = color_invert(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl)));
		vLineSegments.emplace_back(LineStartPos, BasePos + QuantizedDirection * HookLength);
	}

	// add a line from the player to the start position to prevent a visual gap
	vLineSegments.emplace_back(Position, Position + StartOffset);

	if(AlwaysRenderHookColl && RenderHookCollPlayer)
	{
		// invert the hook coll colors when using cl_show_hook_coll_always and +showhookcoll is pressed
		HookCollColor = color_invert(HookCollColor);
	}

	// Render hook coll line
	const int HookCollSize = Local ? g_Config.m_ClHookCollSize : g_Config.m_ClHookCollSizeOther;

	float Alpha = GameClient()->IsOtherTeam(ClientId) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	Alpha *= (float)g_Config.m_ClHookCollAlpha / 100;
	if(Alpha <= 0.0f)
		return;

	Graphics()->TextureClear();
	if(HookCollSize > 0)
	{
		std::vector<IGraphics::CFreeformItem> vLineQuadSegments;
		vLineQuadSegments.reserve(vLineSegments.size());

		float LineWidth = 0.5f + (float)(HookCollSize - 1) * 0.25f;
		const vec2 PerpToAngle = normalize(vec2(Direction.y, -Direction.x)) * GameClient()->m_Camera.m_Zoom;

		for(const auto &LineSegment : vLineSegments)
		{
			vec2 DrawInitPos(LineSegment.m_X0, LineSegment.m_Y0);
			vec2 DrawFinishPos(LineSegment.m_X1, LineSegment.m_Y1);
			vec2 Pos0 = DrawFinishPos + PerpToAngle * -LineWidth;
			vec2 Pos1 = DrawFinishPos + PerpToAngle * LineWidth;
			vec2 Pos2 = DrawInitPos + PerpToAngle * -LineWidth;
			vec2 Pos3 = DrawInitPos + PerpToAngle * LineWidth;
			vLineQuadSegments.emplace_back(Pos0.x, Pos0.y, Pos1.x, Pos1.y, Pos2.x, Pos2.y, Pos3.x, Pos3.y);
		}
		Graphics()->QuadsBegin();
		Graphics()->SetColor(HookCollColor.WithAlpha(Alpha));
		Graphics()->QuadsDrawFreeform(vLineQuadSegments.data(), vLineQuadSegments.size());
		Graphics()->QuadsEnd();
	}
	else
	{
		Graphics()->LinesBegin();
		Graphics()->SetColor(HookCollColor.WithAlpha(Alpha));
		Graphics()->LinesDraw(vLineSegments.data(), vLineSegments.size());
		Graphics()->LinesEnd();
	}
}

void CPlayers::RenderHook(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientId,
	float Intra)
{
	if(pPrevChar->m_HookState <= 0 || pPlayerChar->m_HookState <= 0)
		return;

	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = *pRenderInfo;

	// don't render hooks to not active character cores
	if(pPlayerChar->m_HookedPlayer != -1 && !GameClient()->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Active)
		return;

	if(ClientId >= 0)
		Intra = GameClient()->m_aClients[ClientId].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);

	bool OtherTeam = GameClient()->IsOtherTeam(ClientId);
	float Alpha = (OtherTeam || ClientId < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(ClientId == -2) // ghost
		Alpha = g_Config.m_ClRaceGhostAlpha / 100.0f;

	RenderInfo.m_Size = 64.0f;

	vec2 Position;
	if(in_range(ClientId, MAX_CLIENTS - 1))
		Position = GameClient()->m_aClients[ClientId].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), Intra);

	// draw hook
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	if(ClientId < 0)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

	vec2 Pos = Position;
	vec2 HookPos;

	if(in_range(pPlayerChar->m_HookedPlayer, MAX_CLIENTS - 1))
		HookPos = GameClient()->m_aClients[pPlayerChar->m_HookedPlayer].m_RenderPos;
	else
		HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), Intra);

	float d = distance(Pos, HookPos);
	vec2 Dir = normalize(Pos - HookPos);

	Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHookHead);
	Graphics()->QuadsSetRotation(angle(Dir) + pi);
	// render head
	int QuadOffset = NUM_WEAPONS * 2 + 2;
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookPos.x, HookPos.y);

	// render chain
	++QuadOffset;
	static IGraphics::SRenderSpriteInfo s_aHookChainRenderInfo[1024];
	int HookChainCount = 0;
	for(float f = 24; f < d && HookChainCount < 1024; f += 24, ++HookChainCount)
	{
		vec2 p = HookPos + Dir * f;
		s_aHookChainRenderInfo[HookChainCount].m_Pos[0] = p.x;
		s_aHookChainRenderInfo[HookChainCount].m_Pos[1] = p.y;
		s_aHookChainRenderInfo[HookChainCount].m_Scale = 1;
		s_aHookChainRenderInfo[HookChainCount].m_Rotation = angle(Dir) + pi;
	}
	Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHookChain);
	Graphics()->RenderQuadContainerAsSpriteMultiple(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookChainCount, s_aHookChainRenderInfo);

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	RenderHand(&RenderInfo, Position, normalize(HookPos - Pos), -pi / 2, vec2(20, 0), Alpha);
}

void CPlayers::RenderPlayer(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientId,
	float Intra)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = *pRenderInfo;

	bool Local = GameClient()->m_Snap.m_LocalClientId == ClientId;
	bool OtherTeam = GameClient()->IsOtherTeam(ClientId);
	float Alpha = (OtherTeam || ClientId < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(ClientId == -2) // ghost
		Alpha = g_Config.m_ClRaceGhostAlpha / 100.0f;

	// set size
	RenderInfo.m_Size = 64.0f;

	if(ClientId >= 0)
		Intra = GameClient()->m_aClients[ClientId].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);

	static float s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);
	static float s_LastPredIntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
	if(GameClient()->m_Snap.m_pGameInfoObj && !(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
	{
		s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);
		s_LastPredIntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
	}

	bool PredictLocalWeapons = false;
	float AttackTime = (Client()->PrevGameTick(g_Config.m_ClDummy) - Player.m_AttackTick) / (float)Client()->GameTickSpeed() + Client()->GameTickTime(g_Config.m_ClDummy);
	float LastAttackTime = (Client()->PrevGameTick(g_Config.m_ClDummy) - Player.m_AttackTick) / (float)Client()->GameTickSpeed() + s_LastGameTickTime;
	if(ClientId >= 0 && GameClient()->m_aClients[ClientId].m_IsPredictedLocal && GameClient()->AntiPingGunfire())
	{
		PredictLocalWeapons = true;
		AttackTime = (Client()->PredIntraGameTick(g_Config.m_ClDummy) + (Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Player.m_AttackTick)) / (float)Client()->GameTickSpeed();
		LastAttackTime = (s_LastPredIntraTick + (Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Player.m_AttackTick)) / (float)Client()->GameTickSpeed();
	}
	float AttackTicksPassed = AttackTime * (float)Client()->GameTickSpeed();

	float Angle = GetPlayerTargetAngle(&Prev, &Player, ClientId, Intra);

	vec2 Direction = direction(Angle);
	vec2 Position;
	if(in_range(ClientId, MAX_CLIENTS - 1))
		Position = GameClient()->m_aClients[ClientId].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), Intra);
	vec2 Vel = mix(vec2(Prev.m_VelX / 256.0f, Prev.m_VelY / 256.0f), vec2(Player.m_VelX / 256.0f, Player.m_VelY / 256.0f), Intra);

	GameClient()->m_Flow.Add(Position, Vel * 100.0f, 10.0f);

	RenderInfo.m_GotAirJump = Player.m_Jumped & 2 ? false : true;

	RenderInfo.m_FeetFlipped = false;

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y + 16);
	bool Running = Player.m_VelX >= 5000 || Player.m_VelX <= -5000;
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);
	bool Inactive = ClientId >= 0 && (GameClient()->m_aClients[ClientId].m_Afk || GameClient()->m_aClients[ClientId].m_Paused);

	// evaluate animation
	float WalkTime = std::fmod(Position.x, 100.0f) / 100.0f;
	float RunTime = std::fmod(Position.x, 200.0f) / 200.0f;

	// Don't do a moon walk outside the left border
	if(WalkTime < 0.0f)
		WalkTime += 1.0f;
	if(RunTime < 0.0f)
		RunTime += 1.0f;

	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0.0f, 1.0f); // TODO: some sort of time here
	else if(Stationary)
	{
		if(Inactive)
		{
			State.Add(Direction.x < 0.0f ? &g_pData->m_aAnimations[ANIM_SIT_LEFT] : &g_pData->m_aAnimations[ANIM_SIT_RIGHT], 0.0f, 1.0f); // TODO: some sort of time here
			RenderInfo.m_FeetFlipped = true;
		}
		else
			State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f); // TODO: some sort of time here
	}
	else if(!WantOtherDir)
	{
		if(Running)
			State.Add(Player.m_VelX < 0 ? &g_pData->m_aAnimations[ANIM_RUN_LEFT] : &g_pData->m_aAnimations[ANIM_RUN_RIGHT], RunTime, 1.0f);
		else
			State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);
	}

	if(Player.m_Weapon == WEAPON_HAMMER)
		State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], std::clamp(LastAttackTime * 5.0f, 0.0f, 1.0f), 1.0f);
	if(Player.m_Weapon == WEAPON_NINJA)
		State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], std::clamp(LastAttackTime * 2.0f, 0.0f, 1.0f), 1.0f);

	// do skidding
	if(!InAir && WantOtherDir && length(Vel * 50) > 500.0f)
		GameClient()->m_Effects.SkidTrail(Position, Vel, Player.m_Direction, Alpha);

	// draw gun
	if(Player.m_Weapon >= 0)
	{
		if(!(RenderInfo.m_TeeRenderFlags & TEE_NO_WEAPON))
		{
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);

			// normal weapons
			int CurrentWeapon = std::clamp(Player.m_Weapon, 0, NUM_WEAPONS - 1);
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeapons[CurrentWeapon]);
			int QuadOffset = CurrentWeapon * 2 + (Direction.x < 0.0f ? 1 : 0);

			float Recoil = 0.0f;
			vec2 WeaponPosition;
			bool IsSit = Inactive && !InAir && Stationary;

			if(Player.m_Weapon == WEAPON_HAMMER)
			{
				// static position for hammer
				WeaponPosition = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
				WeaponPosition.y += g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsety;
				if(Direction.x < 0.0f)
					WeaponPosition.x -= g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsetx;
				if(IsSit)
					WeaponPosition.y += 3.0f;

				// if active and attack is under way, bash stuffs
				if(!Inactive || LastAttackTime < GameClient()->m_aClients[ClientId].m_Predicted.m_Tuning.GetWeaponFireDelay(Player.m_Weapon))
				{
					if(Direction.x < 0)
						Graphics()->QuadsSetRotation(-pi / 2.0f - State.GetAttach()->m_Angle * pi * 2.0f);
					else
						Graphics()->QuadsSetRotation(-pi / 2.0f + State.GetAttach()->m_Angle * pi * 2.0f);
				}
				else
					Graphics()->QuadsSetRotation(Direction.x < 0.0f ? 100.0f : 500.0f);

				Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);
			}
			else if(Player.m_Weapon == WEAPON_NINJA)
			{
				WeaponPosition = Position;
				WeaponPosition.y += g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsety;
				if(IsSit)
					WeaponPosition.y += 3.0f;

				if(Direction.x < 0.0f)
				{
					Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2.0f);
					WeaponPosition.x -= g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsetx;
					GameClient()->m_Effects.PowerupShine(WeaponPosition + vec2(32.0f, 0.0f), vec2(32.0f, 12.0f), Alpha);
				}
				else
				{
					Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2.0f);
					GameClient()->m_Effects.PowerupShine(WeaponPosition - vec2(32.0f, 0.0f), vec2(32.0f, 12.0f), Alpha);
				}
				Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);

				// HADOKEN
				if(AttackTime <= 1.0f / 6.0f && g_pData->m_Weapons.m_aId[CurrentWeapon].m_NumSpriteMuzzles)
				{
					int IteX = rand() % g_pData->m_Weapons.m_aId[CurrentWeapon].m_NumSpriteMuzzles;
					static int s_LastIteX = IteX;
					if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
					{
						const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
						if(pInfo->m_Paused)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					else
					{
						if(GameClient()->m_Snap.m_pGameInfoObj && GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					if(g_pData->m_Weapons.m_aId[CurrentWeapon].m_aSpriteMuzzles[IteX])
					{
						vec2 HadokenDirection;
						if(PredictLocalWeapons || ClientId < 0)
							HadokenDirection = vec2(pPlayerChar->m_X, pPlayerChar->m_Y) - vec2(pPrevChar->m_X, pPrevChar->m_Y);
						else
							HadokenDirection = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y) - vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y);
						float HadokenAngle = 0.0f;
						if(absolute(HadokenDirection.x) > 0.0001f || absolute(HadokenDirection.y) > 0.0001f)
						{
							HadokenDirection = normalize(HadokenDirection);
							HadokenAngle = angle(HadokenDirection);
						}
						else
						{
							HadokenDirection = vec2(1.0f, 0.0f);
						}
						Graphics()->QuadsSetRotation(HadokenAngle);
						QuadOffset = IteX * 2;
						WeaponPosition = Position;
						float OffsetX = g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleoffsetx;
						WeaponPosition -= HadokenDirection * OffsetX;
						Graphics()->TextureSet(GameClient()->m_GameSkin.m_aaSpriteWeaponsMuzzles[CurrentWeapon][IteX]);
						Graphics()->RenderQuadContainerAsSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[CurrentWeapon], QuadOffset, WeaponPosition.x, WeaponPosition.y);
					}
				}
			}
			else
			{
				// TODO: should be an animation
				Recoil = 0.0f;
				float a = AttackTicksPassed / 5.0f;
				if(a < 1.0f)
					Recoil = std::sin(a * pi);
				WeaponPosition = Position + Direction * g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsetx - Direction * Recoil * 10.0f;
				WeaponPosition.y += g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsety;
				if(IsSit)
					WeaponPosition.y += 3.0f;
				if(Player.m_Weapon == WEAPON_GUN && g_Config.m_ClOldGunPosition)
					WeaponPosition.y -= 8.0f;
				Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle * pi * 2.0f + Angle);
				Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);
			}

			if(Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN)
			{
				// check if we're firing stuff
				if(g_pData->m_Weapons.m_aId[CurrentWeapon].m_NumSpriteMuzzles) // prev.attackticks)
				{
					float AlphaMuzzle = 0.0f;
					if(AttackTicksPassed < g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleduration + 3.0f)
					{
						float t = AttackTicksPassed / g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleduration;
						AlphaMuzzle = mix(2.0f, 0.0f, minimum(1.0f, maximum(0.0f, t)));
					}

					int IteX = rand() % g_pData->m_Weapons.m_aId[CurrentWeapon].m_NumSpriteMuzzles;
					static int s_LastIteX = IteX;
					if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
					{
						const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
						if(pInfo->m_Paused)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					else
					{
						if(GameClient()->m_Snap.m_pGameInfoObj && GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					if(AlphaMuzzle > 0.0f && g_pData->m_Weapons.m_aId[CurrentWeapon].m_aSpriteMuzzles[IteX])
					{
						float OffsetY = -g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleoffsety;
						QuadOffset = IteX * 2 + (Direction.x < 0.0f ? 1 : 0);
						if(Direction.x < 0.0f)
							OffsetY = -OffsetY;

						vec2 DirectionY(-Direction.y, Direction.x);
						vec2 MuzzlePos = WeaponPosition + Direction * g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleoffsetx + DirectionY * OffsetY;
						Graphics()->TextureSet(GameClient()->m_GameSkin.m_aaSpriteWeaponsMuzzles[CurrentWeapon][IteX]);
						Graphics()->RenderQuadContainerAsSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[CurrentWeapon], QuadOffset, MuzzlePos.x, MuzzlePos.y);
					}
				}
			}
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(0.0f);

			switch(Player.m_Weapon)
			{
			case WEAPON_GUN: RenderHand(&RenderInfo, WeaponPosition, Direction, -3.0f * pi / 4.0f, vec2(-15.0f, 4.0f), Alpha); break;
			case WEAPON_SHOTGUN: RenderHand(&RenderInfo, WeaponPosition, Direction, -pi / 2.0f, vec2(-5.0f, 4.0f), Alpha); break;
			case WEAPON_GRENADE: RenderHand(&RenderInfo, WeaponPosition, Direction, -pi / 2.0f, vec2(-4.0f, 7.0f), Alpha); break;
			}
		}
	}

	// render the "shadow" tee
	if(Local && ((g_Config.m_Debug && g_Config.m_ClUnpredictedShadow >= 0) || g_Config.m_ClUnpredictedShadow == 1))
	{
		vec2 ShadowPosition = Position;
		if(ClientId >= 0)
			ShadowPosition = mix(
				vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y),
				vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y),
				Client()->IntraGameTick(g_Config.m_ClDummy));

		RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, ShadowPosition, 0.5f); // render ghost
	}

	RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position, Alpha);

	float TeeAnimScale, TeeBaseSize;
	CRenderTools::GetRenderTeeAnimScaleAndBaseSize(&RenderInfo, TeeAnimScale, TeeBaseSize);
	vec2 BodyPos = Position + vec2(State.GetBody()->m_X, State.GetBody()->m_Y) * TeeAnimScale;
	if(RenderInfo.m_TeeRenderFlags & TEE_EFFECT_FROZEN)
	{
		GameClient()->m_Effects.FreezingFlakes(BodyPos, vec2(32, 32), Alpha);
	}
	if(RenderInfo.m_TeeRenderFlags & TEE_EFFECT_SPARKLE)
	{
		GameClient()->m_Effects.SparkleTrail(BodyPos, Alpha);
	}

	if(ClientId < 0)
		return;

	int QuadOffsetToEmoticon = NUM_WEAPONS * 2 + 2 + 2;
	if((Player.m_PlayerFlags & PLAYERFLAG_CHATTING) && !GameClient()->m_aClients[ClientId].m_Afk)
	{
		int CurEmoticon = (SPRITE_DOTDOT - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[CurEmoticon]);
		int QuadOffset = QuadOffsetToEmoticon + CurEmoticon;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClAfkEmote && GameClient()->m_aClients[ClientId].m_Afk && ClientId != GameClient()->m_aLocalIds[!g_Config.m_ClDummy])
	{
		int CurEmoticon = (SPRITE_ZZZ - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[CurEmoticon]);
		int QuadOffset = QuadOffsetToEmoticon + CurEmoticon;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClShowEmotes && !GameClient()->m_aClients[ClientId].m_EmoticonIgnore && GameClient()->m_aClients[ClientId].m_EmoticonStartTick != -1)
	{
		float SinceStart = (Client()->GameTick(g_Config.m_ClDummy) - GameClient()->m_aClients[ClientId].m_EmoticonStartTick) + (Client()->IntraGameTickSincePrev(g_Config.m_ClDummy) - GameClient()->m_aClients[ClientId].m_EmoticonStartFraction);
		float FromEnd = (2 * Client()->GameTickSpeed()) - SinceStart;

		if(0 <= SinceStart && FromEnd > 0)
		{
			float a = 1;

			if(FromEnd < Client()->GameTickSpeed() / 5)
				a = FromEnd / (Client()->GameTickSpeed() / 5.0f);

			float h = 1;
			if(SinceStart < Client()->GameTickSpeed() / 10)
				h = SinceStart / (Client()->GameTickSpeed() / 10.0f);

			float Wiggle = 0;
			if(SinceStart < Client()->GameTickSpeed() / 5)
				Wiggle = SinceStart / (Client()->GameTickSpeed() / 5.0f);

			float WiggleAngle = std::sin(5 * Wiggle);

			Graphics()->QuadsSetRotation(pi / 6 * WiggleAngle);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, a * Alpha);
			// client_datas::emoticon is an offset from the first emoticon
			int QuadOffset = QuadOffsetToEmoticon + GameClient()->m_aClients[ClientId].m_Emoticon;
			Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[GameClient()->m_aClients[ClientId].m_Emoticon]);
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x, Position.y - 23.f - 32.f * h, 1.f, (64.f * h) / 64.f);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(0);
		}
	}
}

inline bool CPlayers::IsPlayerInfoAvailable(int ClientId) const
{
	return GameClient()->m_Snap.m_aCharacters[ClientId].m_Active &&
	       GameClient()->m_Snap.m_apPrevPlayerInfos[ClientId] != nullptr &&
	       GameClient()->m_Snap.m_apPlayerInfos[ClientId] != nullptr;
}

void CPlayers::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	// update render info for ninja
	CTeeRenderInfo aRenderInfo[MAX_CLIENTS];
	const bool IsTeamPlay = GameClient()->IsTeamPlay();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		aRenderInfo[i] = GameClient()->m_aClients[i].m_RenderInfo;
		aRenderInfo[i].m_TeeRenderFlags = 0;

		// predict freeze skin only for local players
		bool Frozen = false;
		if(i == GameClient()->m_aLocalIds[0] || i == GameClient()->m_aLocalIds[1])
		{
			if(GameClient()->m_aClients[i].m_Predicted.m_FreezeEnd != 0)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
			if(GameClient()->m_aClients[i].m_Predicted.m_LiveFrozen)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN;
			if(GameClient()->m_aClients[i].m_Predicted.m_Invincible)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_SPARKLE;

			Frozen = GameClient()->m_aClients[i].m_Predicted.m_FreezeEnd != 0;
		}
		else
		{
			if(GameClient()->m_aClients[i].m_FreezeEnd != 0)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
			if(GameClient()->m_aClients[i].m_LiveFrozen)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN;
			if(GameClient()->m_aClients[i].m_Invincible)
				aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_SPARKLE;

			Frozen = GameClient()->m_Snap.m_aCharacters[i].m_HasExtendedData && GameClient()->m_Snap.m_aCharacters[i].m_ExtendedData.m_FreezeEnd != 0;
		}

		if((GameClient()->m_aClients[i].m_RenderCur.m_Weapon == WEAPON_NINJA || (Frozen && !GameClient()->m_GameInfo.m_NoSkinChangeForFrozen)) && g_Config.m_ClShowNinja)
		{
			// change the skin for the player to the ninja
			aRenderInfo[i].m_aSixup[g_Config.m_ClDummy].Reset();
			aRenderInfo[i].ApplySkin(NinjaTeeRenderInfo()->TeeRenderInfo());
			aRenderInfo[i].m_CustomColoredSkin = IsTeamPlay;
			if(!IsTeamPlay)
			{
				aRenderInfo[i].m_ColorBody = ColorRGBA(1, 1, 1);
				aRenderInfo[i].m_ColorFeet = ColorRGBA(1, 1, 1);
			}
		}
	}

	// get screen edges to avoid rendering offscreen
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	// expand the edges to prevent popping in/out onscreen
	//
	// it is assumed that the tee, all its weapons, and emotes fit into a 200x200 box centered on the tee
	// this may need to be changed or calculated differently in the future
	float BorderBuffer = 100;
	ScreenX0 -= BorderBuffer;
	ScreenX1 += BorderBuffer;
	ScreenY0 -= BorderBuffer;
	ScreenY1 += BorderBuffer;

	// render everyone else's hook, then our own
	const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == LocalClientId || !IsPlayerInfoAvailable(ClientId))
		{
			continue;
		}
		RenderHook(&GameClient()->m_aClients[ClientId].m_RenderPrev, &GameClient()->m_aClients[ClientId].m_RenderCur, &aRenderInfo[ClientId], ClientId);
	}
	if(LocalClientId != -1 && IsPlayerInfoAvailable(LocalClientId))
	{
		const CGameClient::CClientData *pLocalClientData = &GameClient()->m_aClients[LocalClientId];
		RenderHook(&pLocalClientData->m_RenderPrev, &pLocalClientData->m_RenderCur, &aRenderInfo[LocalClientId], LocalClientId);
	}

	// render spectating players
	for(const auto &Client : GameClient()->m_aClients)
	{
		if(!Client.m_SpecCharPresent)
		{
			continue;
		}

		const int ClientId = Client.ClientId();
		float Alpha = (GameClient()->IsOtherTeam(ClientId) || ClientId < 0) ? g_Config.m_ClShowOthersAlpha / 100.f : 1.f;
		if(ClientId == -2) // ghost
		{
			Alpha = g_Config.m_ClRaceGhostAlpha / 100.f;
		}
		RenderTools()->RenderTee(CAnimState::GetIdle(), &SpectatorTeeRenderInfo()->TeeRenderInfo(), EMOTE_BLINK, vec2(1, 0), Client.m_SpecChar, Alpha);
	}

	// render everyone else's tee, then either our own or the tee we are spectating.
	const int RenderLastId = (GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW && GameClient()->m_Snap.m_SpecInfo.m_Active) ? GameClient()->m_Snap.m_SpecInfo.m_SpectatorId : LocalClientId;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == RenderLastId || !IsPlayerInfoAvailable(ClientId))
		{
			continue;
		}

		RenderHookCollLine(&GameClient()->m_aClients[ClientId].m_RenderPrev, &GameClient()->m_aClients[ClientId].m_RenderCur, ClientId);

		if(!in_range(GameClient()->m_aClients[ClientId].m_RenderPos.x, ScreenX0, ScreenX1) || !in_range(GameClient()->m_aClients[ClientId].m_RenderPos.y, ScreenY0, ScreenY1))
		{
			continue;
		}
		RenderPlayer(&GameClient()->m_aClients[ClientId].m_RenderPrev, &GameClient()->m_aClients[ClientId].m_RenderCur, &aRenderInfo[ClientId], ClientId);
	}
	if(RenderLastId != -1 && IsPlayerInfoAvailable(RenderLastId))
	{
		const CGameClient::CClientData *pClientData = &GameClient()->m_aClients[RenderLastId];
		RenderHookCollLine(&pClientData->m_RenderPrev, &pClientData->m_RenderCur, RenderLastId);
		RenderPlayer(&pClientData->m_RenderPrev, &pClientData->m_RenderCur, &aRenderInfo[RenderLastId], RenderLastId);
	}
}

void CPlayers::CreateNinjaTeeRenderInfo()
{
	CTeeRenderInfo NinjaTeeRenderInfo;
	NinjaTeeRenderInfo.m_Size = 64.0f;
	CSkinDescriptor NinjaSkinDescriptor;
	NinjaSkinDescriptor.m_Flags |= CSkinDescriptor::FLAG_SIX;
	str_copy(NinjaSkinDescriptor.m_aSkinName, "x_ninja");
	m_pNinjaTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(NinjaTeeRenderInfo, NinjaSkinDescriptor);
}

void CPlayers::CreateSpectatorTeeRenderInfo()
{
	CTeeRenderInfo SpectatorTeeRenderInfo;
	SpectatorTeeRenderInfo.m_Size = 64.0f;
	CSkinDescriptor SpectatorSkinDescriptor;
	SpectatorSkinDescriptor.m_Flags |= CSkinDescriptor::FLAG_SIX;
	str_copy(SpectatorSkinDescriptor.m_aSkinName, "x_spec");
	m_pSpectatorTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(SpectatorTeeRenderInfo, SpectatorSkinDescriptor);
}

void CPlayers::OnInit()
{
	m_WeaponEmoteQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY);
		Graphics()->QuadsSetSubset(0, 1, 1, 0);
		Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY);
	}
	float ScaleX, ScaleY;

	// at the end the hand
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);

	for(int i = 0; i < NUM_EMOTICONS; ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 64.f);
	}
	Graphics()->QuadContainerUpload(m_WeaponEmoteQuadContainerIndex);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		m_aWeaponSpriteMuzzleQuadContainerIndex[i] = Graphics()->CreateQuadContainer(false);
		for(int n = 0; n < g_pData->m_Weapons.m_aId[i].m_NumSpriteMuzzles; ++n)
		{
			if(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n])
			{
				if(i == WEAPON_GUN || i == WEAPON_SHOTGUN)
				{
					// TODO: hardcoded for now to get the same particle size as before
					Graphics()->GetSpriteScaleImpl(96, 64, ScaleX, ScaleY);
				}
				else
					Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n], ScaleX, ScaleY);
			}

			float SWidth = (g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX) * (4.0f / 3.0f);
			float SHeight = g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY;

			Graphics()->QuadsSetSubset(0, 0, 1, 1);
			if(WEAPON_NINJA == i)
				Graphics()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], 160.f * ScaleX, 160.f * ScaleY);
			else
				Graphics()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], SWidth, SHeight);

			Graphics()->QuadsSetSubset(0, 1, 1, 0);
			if(WEAPON_NINJA == i)
				Graphics()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], 160.f * ScaleX, 160.f * ScaleY);
			else
				Graphics()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], SWidth, SHeight);
		}
		Graphics()->QuadContainerUpload(m_aWeaponSpriteMuzzleQuadContainerIndex[i]);
	}

	Graphics()->QuadsSetSubset(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0.f);

	CreateNinjaTeeRenderInfo();
	CreateSpectatorTeeRenderInfo();
}
