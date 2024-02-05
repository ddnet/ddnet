/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/gamecore.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/mapitems.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <game/client/components/controls.h>
#include <game/client/components/effects.h>
#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>

#include "players.h"

#include <base/color.h>
#include <base/math.h>

void CPlayers::RenderHand(const CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset, float Alpha)
{
	vec2 HandPos = CenterPos + Dir;
	float Angle = angle(Dir);
	if(Dir.x < 0)
		Angle -= AngleOffset;
	else
		Angle += AngleOffset;

	vec2 DirX = Dir;
	vec2 DirY(-Dir.y, Dir.x);

	if(Dir.x < 0)
		DirY = -DirY;

	HandPos += DirX * PostRotOffset.x;
	HandPos += DirY * PostRotOffset.y;

	const CSkin::SSkinTextures *pSkinTextures = pInfo->m_CustomColoredSkin ? &pInfo->m_ColorableRenderSkin : &pInfo->m_OriginalRenderSkin;

	Graphics()->SetColor(pInfo->m_ColorBody.r, pInfo->m_ColorBody.g, pInfo->m_ColorBody.b, Alpha);

	// two passes
	for(int i = 0; i < 2; i++)
	{
		int QuadOffset = NUM_WEAPONS * 2 + i;
		Graphics()->QuadsSetRotation(Angle);
		Graphics()->TextureSet(i == 0 ? pSkinTextures->m_HandsOutline : pSkinTextures->m_Hands);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, HandPos.x, HandPos.y);
	}
}

inline float AngularMixDirection(float Src, float Dst) { return std::sin(Dst - Src) > 0 ? 1 : -1; }

inline float AngularApproach(float Src, float Dst, float Amount)
{
	float d = AngularMixDirection(Src, Dst);
	float n = Src + Amount * d;
	if(AngularMixDirection(n, Dst) != d)
		return Dst;
	return n;
}

float CPlayers::GetPlayerTargetAngle(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientID,
	float Intra)
{
	float AngleIntraTick = Intra;
	// using unpredicted angle when rendering other players in-game
	if(ClientID >= 0)
		AngleIntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
	if(ClientID >= 0 && m_pClient->m_Snap.m_aCharacters[ClientID].m_HasExtendedDisplayInfo)
	{
		CNetObj_DDNetCharacter *pExtendedData = &m_pClient->m_Snap.m_aCharacters[ClientID].m_ExtendedData;
		if(m_pClient->m_Snap.m_aCharacters[ClientID].m_PrevExtendedData)
		{
			const CNetObj_DDNetCharacter *PrevExtendedData = m_pClient->m_Snap.m_aCharacters[ClientID].m_PrevExtendedData;

			float MixX = mix((float)PrevExtendedData->m_TargetX, (float)pExtendedData->m_TargetX, AngleIntraTick);
			float MixY = mix((float)PrevExtendedData->m_TargetY, (float)pExtendedData->m_TargetY, AngleIntraTick);

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
			return mix((float)pPrevChar->m_Angle, (float)(pPlayerChar->m_Angle - 256.0f * 2 * pi), AngleIntraTick) / 256.0f;
		}
		else if(pPlayerChar->m_Angle < 0 && pPrevChar->m_Angle > (256.0f * pi))
		{
			return mix((float)pPrevChar->m_Angle, (float)(pPlayerChar->m_Angle + 256.0f * 2 * pi), AngleIntraTick) / 256.0f;
		}
		else
		{
			return mix((float)pPrevChar->m_Angle, (float)pPlayerChar->m_Angle, AngleIntraTick) / 256.0f;
		}
	}
}

void CPlayers::RenderHookCollLine(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientID,
	float Intra)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	bool Local = m_pClient->m_Snap.m_LocalClientID == ClientID;
	bool OtherTeam = m_pClient->IsOtherTeam(ClientID);
	float Alpha = (OtherTeam || ClientID < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	Alpha *= (float)g_Config.m_ClHookCollAlpha / 100;

	float IntraTick = Intra;
	if(ClientID >= 0)
		IntraTick = m_pClient->m_aClients[ClientID].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);

	float Angle;
	if(Local && (!m_pClient->m_Snap.m_SpecInfo.m_Active || m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW) && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's the local player we are rendering
		Angle = angle(m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] * m_pClient->m_Camera.m_Zoom);
	}
	else
	{
		Angle = GetPlayerTargetAngle(&Prev, &Player, ClientID, IntraTick);
	}

	vec2 Direction = direction(Angle);
	vec2 Position;
	if(in_range(ClientID, MAX_CLIENTS - 1))
		Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	// draw hook collision line
	{
		bool AlwaysRenderHookColl = GameClient()->m_GameInfo.m_AllowHookColl && (Local ? g_Config.m_ClShowHookCollOwn : g_Config.m_ClShowHookCollOther) == 2;
		bool RenderHookCollPlayer = ClientID >= 0 && Player.m_PlayerFlags & PLAYERFLAG_AIM && (Local ? g_Config.m_ClShowHookCollOwn : g_Config.m_ClShowHookCollOther) > 0;
		if(Local && GameClient()->m_GameInfo.m_AllowHookColl && Client()->State() != IClient::STATE_DEMOPLAYBACK)
			RenderHookCollPlayer = GameClient()->m_Controls.m_aShowHookColl[g_Config.m_ClDummy] && g_Config.m_ClShowHookCollOwn > 0;

		bool RenderHookCollVideo = true;
#if defined(CONF_VIDEORECORDER)
		RenderHookCollVideo = !IVideo::Current() || g_Config.m_ClVideoShowHookCollOther || Local;
#endif
		if((AlwaysRenderHookColl || RenderHookCollPlayer) && RenderHookCollVideo)
		{
			vec2 ExDirection = Direction;

			if(Local && (!m_pClient->m_Snap.m_SpecInfo.m_Active || m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW) && Client()->State() != IClient::STATE_DEMOPLAYBACK)
			{
				ExDirection = normalize(
					vec2((int)((int)m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].x * m_pClient->m_Camera.m_Zoom),
						(int)((int)m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].y * m_pClient->m_Camera.m_Zoom)));

				// fix direction if mouse is exactly in the center
				if(!(int)m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].x && !(int)m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy].y)
					ExDirection = vec2(1, 0);
			}
			Graphics()->TextureClear();
			vec2 InitPos = Position;
			vec2 FinishPos = InitPos + ExDirection * (m_pClient->m_aTuning[g_Config.m_ClDummy].m_HookLength - 42.0f);

			if(g_Config.m_ClHookCollSize > 0)
				Graphics()->QuadsBegin();
			else
				Graphics()->LinesBegin();

			ColorRGBA HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorNoColl));

			vec2 OldPos = InitPos + ExDirection * CCharacterCore::PhysicalSize() * 1.5f;
			vec2 NewPos = OldPos;

			bool DoBreak = false;

			do
			{
				OldPos = NewPos;
				NewPos = OldPos + ExDirection * m_pClient->m_aTuning[g_Config.m_ClDummy].m_HookFireSpeed;

				if(distance(InitPos, NewPos) > m_pClient->m_aTuning[g_Config.m_ClDummy].m_HookLength)
				{
					NewPos = InitPos + normalize(NewPos - InitPos) * m_pClient->m_aTuning[g_Config.m_ClDummy].m_HookLength;
					DoBreak = true;
				}

				int Hit = Collision()->IntersectLineTeleHook(OldPos, NewPos, &FinishPos, 0x0);

				if(!DoBreak && Hit)
				{
					if(Hit != TILE_NOHOOK)
					{
						HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorHookableColl));
					}
				}

				if(m_pClient->IntersectCharacter(OldPos, FinishPos, FinishPos, ClientID) != -1)
				{
					HookCollColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClHookCollColorTeeColl));
					break;
				}

				if(Hit)
					break;

				NewPos.x = round_to_int(NewPos.x);
				NewPos.y = round_to_int(NewPos.y);

				if(OldPos == NewPos)
					break;

				ExDirection.x = round_to_int(ExDirection.x * 256.0f) / 256.0f;
				ExDirection.y = round_to_int(ExDirection.y * 256.0f) / 256.0f;
			} while(!DoBreak);

			if(AlwaysRenderHookColl && RenderHookCollPlayer)
			{
				// invert the hook coll colors when using cl_show_hook_coll_always and +showhookcoll is pressed
				HookCollColor = color_invert(HookCollColor);
			}
			Graphics()->SetColor(HookCollColor.WithAlpha(Alpha));
			if(g_Config.m_ClHookCollSize > 0)
			{
				float LineWidth = 0.5f + (float)(g_Config.m_ClHookCollSize - 1) * 0.25f;
				vec2 PerpToAngle = normalize(vec2(ExDirection.y, -ExDirection.x)) * GameClient()->m_Camera.m_Zoom;
				vec2 Pos0 = FinishPos + PerpToAngle * -LineWidth;
				vec2 Pos1 = FinishPos + PerpToAngle * LineWidth;
				vec2 Pos2 = InitPos + PerpToAngle * -LineWidth;
				vec2 Pos3 = InitPos + PerpToAngle * LineWidth;
				IGraphics::CFreeformItem FreeformItem(Pos0.x, Pos0.y, Pos1.x, Pos1.y, Pos2.x, Pos2.y, Pos3.x, Pos3.y);
				Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
				Graphics()->QuadsEnd();
			}
			else
			{
				IGraphics::CLineItem LineItem(InitPos.x, InitPos.y, FinishPos.x, FinishPos.y);
				Graphics()->LinesDraw(&LineItem, 1);
				Graphics()->LinesEnd();
			}
		}
	}
}
void CPlayers::RenderHook(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientID,
	float Intra)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = *pRenderInfo;

	// don't render hooks to not active character cores
	if(pPlayerChar->m_HookedPlayer != -1 && !m_pClient->m_Snap.m_aCharacters[pPlayerChar->m_HookedPlayer].m_Active)
		return;

	float IntraTick = Intra;
	if(ClientID >= 0)
		IntraTick = (m_pClient->m_aClients[ClientID].m_IsPredicted) ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);

	bool OtherTeam = m_pClient->IsOtherTeam(ClientID);
	float Alpha = (OtherTeam || ClientID < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(ClientID == -2) // ghost
		Alpha = g_Config.m_ClRaceGhostAlpha / 100.0f;

	RenderInfo.m_Size = 64.0f;

	vec2 Position;
	if(in_range(ClientID, MAX_CLIENTS - 1))
		Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	// draw hook
	if(Prev.m_HookState > 0 && Player.m_HookState > 0)
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		if(ClientID < 0)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

		vec2 Pos = Position;
		vec2 HookPos;

		if(in_range(pPlayerChar->m_HookedPlayer, MAX_CLIENTS - 1))
			HookPos = m_pClient->m_aClients[pPlayerChar->m_HookedPlayer].m_RenderPos;
		else
			HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), IntraTick);

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
}

void CPlayers::RenderPlayer(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientID,
	float Intra)
{
	CNetObj_Character Prev;
	CNetObj_Character Player;
	Prev = *pPrevChar;
	Player = *pPlayerChar;

	CTeeRenderInfo RenderInfo = *pRenderInfo;

	bool Local = m_pClient->m_Snap.m_LocalClientID == ClientID;
	bool OtherTeam = m_pClient->IsOtherTeam(ClientID);
	float Alpha = (OtherTeam || ClientID < 0) ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
	if(ClientID == -2) // ghost
		Alpha = g_Config.m_ClRaceGhostAlpha / 100.0f;

	// set size
	RenderInfo.m_Size = 64.0f;

	float IntraTick = Intra;
	if(ClientID >= 0)
		IntraTick = m_pClient->m_aClients[ClientID].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);

	static float s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);
	static float s_LastPredIntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
	if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
	{
		s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);
		s_LastPredIntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
	}

	bool PredictLocalWeapons = false;
	float AttackTime = (Client()->PrevGameTick(g_Config.m_ClDummy) - Player.m_AttackTick) / (float)Client()->GameTickSpeed() + Client()->GameTickTime(g_Config.m_ClDummy);
	float LastAttackTime = (Client()->PrevGameTick(g_Config.m_ClDummy) - Player.m_AttackTick) / (float)Client()->GameTickSpeed() + s_LastGameTickTime;
	if(ClientID >= 0 && m_pClient->m_aClients[ClientID].m_IsPredictedLocal && m_pClient->AntiPingGunfire())
	{
		PredictLocalWeapons = true;
		AttackTime = (Client()->PredIntraGameTick(g_Config.m_ClDummy) + (Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Player.m_AttackTick)) / (float)Client()->GameTickSpeed();
		LastAttackTime = (s_LastPredIntraTick + (Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Player.m_AttackTick)) / (float)Client()->GameTickSpeed();
	}
	float AttackTicksPassed = AttackTime * (float)Client()->GameTickSpeed();

	float Angle;
	if(Local && (!m_pClient->m_Snap.m_SpecInfo.m_Active || m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW) && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's the local player we are rendering
		Angle = angle(m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] * m_pClient->m_Camera.m_Zoom);
	}
	else
	{
		Angle = GetPlayerTargetAngle(&Prev, &Player, ClientID, IntraTick);
	}

	vec2 Direction = direction(Angle);
	vec2 Position;
	if(in_range(ClientID, MAX_CLIENTS - 1))
		Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	vec2 Vel = mix(vec2(Prev.m_VelX / 256.0f, Prev.m_VelY / 256.0f), vec2(Player.m_VelX / 256.0f, Player.m_VelY / 256.0f), IntraTick);

	m_pClient->m_Flow.Add(Position, Vel * 100.0f, 10.0f);

	RenderInfo.m_GotAirJump = Player.m_Jumped & 2 ? false : true;

	RenderInfo.m_FeetFlipped = false;

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y + 16);
	bool Running = Player.m_VelX >= 5000 || Player.m_VelX <= -5000;
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);
	bool Inactive = m_pClient->m_aClients[ClientID].m_Afk || m_pClient->m_aClients[ClientID].m_Paused;

	// evaluate animation
	float WalkTime = std::fmod(Position.x, 100.0f) / 100.0f;
	float RunTime = std::fmod(Position.x, 200.0f) / 200.0f;

	// Don't do a moon walk outside the left border
	if(WalkTime < 0)
		WalkTime += 1;
	if(RunTime < 0)
		RunTime += 1;

	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f); // TODO: some sort of time here
	else if(Stationary)
	{
		if(Inactive)
		{
			State.Add(Direction.x < 0 ? &g_pData->m_aAnimations[ANIM_SIT_LEFT] : &g_pData->m_aAnimations[ANIM_SIT_RIGHT], 0, 1.0f); // TODO: some sort of time here
			RenderInfo.m_FeetFlipped = true;
		}
		else
			State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f); // TODO: some sort of time here
	}
	else if(!WantOtherDir)
	{
		if(Running)
			State.Add(Player.m_VelX < 0 ? &g_pData->m_aAnimations[ANIM_RUN_LEFT] : &g_pData->m_aAnimations[ANIM_RUN_RIGHT], RunTime, 1.0f);
		else
			State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);
	}

	if(Player.m_Weapon == WEAPON_HAMMER)
		State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], clamp(LastAttackTime * 5.0f, 0.0f, 1.0f), 1.0f);
	if(Player.m_Weapon == WEAPON_NINJA)
		State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], clamp(LastAttackTime * 2.0f, 0.0f, 1.0f), 1.0f);

	// do skidding
	if(!InAir && WantOtherDir && length(Vel * 50) > 500.0f)
	{
		static int64_t SkidSoundTime = 0;
		if(time() - SkidSoundTime > time_freq() / 10)
		{
			if(g_Config.m_SndGame)
				m_pClient->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SKID, 0.25f, Position);
			SkidSoundTime = time();
		}

		m_pClient->m_Effects.SkidTrail(
			Position + vec2(-Player.m_Direction * 6, 12),
			vec2(-Player.m_Direction * 100 * length(Vel), -50),
			Alpha);
	}

	// draw gun
	{
		if(!(RenderInfo.m_TeeRenderFlags & TEE_NO_WEAPON))
		{
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle * pi * 2 + Angle);

			if(ClientID < 0)
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

			// normal weapons
			int CurrentWeapon = clamp(Player.m_Weapon, 0, NUM_WEAPONS - 1);
			Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeapons[CurrentWeapon]);
			int QuadOffset = CurrentWeapon * 2 + (Direction.x < 0 ? 1 : 0);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);

			vec2 Dir = Direction;
			float Recoil = 0.0f;
			vec2 WeaponPosition;
			bool IsSit = Inactive && !InAir && Stationary;

			if(Player.m_Weapon == WEAPON_HAMMER)
			{
				// static position for hammer
				WeaponPosition = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
				WeaponPosition.y += g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsety;
				if(Direction.x < 0)
					WeaponPosition.x -= g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsetx;
				if(IsSit)
					WeaponPosition.y += 3.0f;

				// if active and attack is under way, bash stuffs
				if(!Inactive || LastAttackTime < m_pClient->m_aTuning[g_Config.m_ClDummy].GetWeaponFireDelay(Player.m_Weapon))
				{
					if(Direction.x < 0)
						Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2);
					else
						Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2);
				}
				else
					Graphics()->QuadsSetRotation(Direction.x < 0 ? 100.0f : 500.0f);

				Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);
			}
			else if(Player.m_Weapon == WEAPON_NINJA)
			{
				WeaponPosition = Position;
				WeaponPosition.y += g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsety;
				if(IsSit)
					WeaponPosition.y += 3.0f;

				if(Direction.x < 0)
				{
					Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2);
					WeaponPosition.x -= g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsetx;
					m_pClient->m_Effects.PowerupShine(WeaponPosition + vec2(32, 0), vec2(32, 12), Alpha);
				}
				else
				{
					Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2);
					m_pClient->m_Effects.PowerupShine(WeaponPosition - vec2(32, 0), vec2(32, 12), Alpha);
				}
				Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);

				// HADOKEN
				if(AttackTime <= 1 / 6.f && g_pData->m_Weapons.m_aId[CurrentWeapon].m_NumSpriteMuzzles)
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
						if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					if(g_pData->m_Weapons.m_aId[CurrentWeapon].m_aSpriteMuzzles[IteX])
					{
						if(PredictLocalWeapons)
							Dir = vec2(pPlayerChar->m_X, pPlayerChar->m_Y) - vec2(pPrevChar->m_X, pPrevChar->m_Y);
						else
							Dir = vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_Y) - vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_Y);
						float HadOkenAngle = 0;
						if(absolute(Dir.x) > 0.0001f || absolute(Dir.y) > 0.0001f)
						{
							Dir = normalize(Dir);
							HadOkenAngle = angle(Dir);
						}
						else
						{
							Dir = vec2(1, 0);
						}
						Graphics()->QuadsSetRotation(HadOkenAngle);
						QuadOffset = IteX * 2;
						vec2 DirY(-Dir.y, Dir.x);
						WeaponPosition = Position;
						float OffsetX = g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleoffsetx;
						WeaponPosition -= Dir * OffsetX;
						Graphics()->TextureSet(GameClient()->m_GameSkin.m_aaSpriteWeaponsMuzzles[CurrentWeapon][IteX]);
						Graphics()->RenderQuadContainerAsSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[CurrentWeapon], QuadOffset, WeaponPosition.x, WeaponPosition.y);
					}
				}
			}
			else
			{
				// TODO: should be an animation
				Recoil = 0;
				float a = AttackTicksPassed / 5.0f;
				if(a < 1)
					Recoil = std::sin(a * pi);
				WeaponPosition = Position + Dir * g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsetx - Dir * Recoil * 10.0f;
				WeaponPosition.y += g_pData->m_Weapons.m_aId[CurrentWeapon].m_Offsety;
				if(IsSit)
					WeaponPosition.y += 3.0f;
				if(Player.m_Weapon == WEAPON_GUN && g_Config.m_ClOldGunPosition)
					WeaponPosition.y -= 8;
				Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, WeaponPosition.x, WeaponPosition.y);
			}

			if(Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN)
			{
				// check if we're firing stuff
				if(g_pData->m_Weapons.m_aId[CurrentWeapon].m_NumSpriteMuzzles) // prev.attackticks)
				{
					float AlphaMuzzle = 0.0f;
					if(AttackTicksPassed < g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleduration + 3)
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
						if(m_pClient->m_Snap.m_pGameInfoObj && m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED)
							IteX = s_LastIteX;
						else
							s_LastIteX = IteX;
					}
					if(AlphaMuzzle > 0.0f && g_pData->m_Weapons.m_aId[CurrentWeapon].m_aSpriteMuzzles[IteX])
					{
						float OffsetY = -g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleoffsety;
						QuadOffset = IteX * 2 + (Direction.x < 0 ? 1 : 0);
						if(Direction.x < 0)
							OffsetY = -OffsetY;

						vec2 DirY(-Dir.y, Dir.x);
						vec2 MuzzlePos = WeaponPosition + Dir * g_pData->m_Weapons.m_aId[CurrentWeapon].m_Muzzleoffsetx + DirY * OffsetY;
						Graphics()->TextureSet(GameClient()->m_GameSkin.m_aaSpriteWeaponsMuzzles[CurrentWeapon][IteX]);
						Graphics()->RenderQuadContainerAsSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[CurrentWeapon], QuadOffset, MuzzlePos.x, MuzzlePos.y);
					}
				}
			}
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(0);

			switch(Player.m_Weapon)
			{
			case WEAPON_GUN: RenderHand(&RenderInfo, WeaponPosition, Direction, -3 * pi / 4, vec2(-15, 4), Alpha); break;
			case WEAPON_SHOTGUN: RenderHand(&RenderInfo, WeaponPosition, Direction, -pi / 2, vec2(-5, 4), Alpha); break;
			case WEAPON_GRENADE: RenderHand(&RenderInfo, WeaponPosition, Direction, -pi / 2, vec2(-4, 7), Alpha); break;
			}
		}
	}

	// render the "shadow" tee
	if(Local && ((g_Config.m_Debug && g_Config.m_ClUnpredictedShadow >= 0) || g_Config.m_ClUnpredictedShadow == 1))
	{
		vec2 ShadowPosition = Position;
		if(ClientID >= 0)
			ShadowPosition = mix(
				vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_Y),
				vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_Y),
				Client()->IntraGameTick(g_Config.m_ClDummy));

		CTeeRenderInfo Shadow = RenderInfo;
		RenderTools()->RenderTee(&State, &Shadow, Player.m_Emote, Direction, ShadowPosition, 0.5f); // render ghost
	}

	RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position, Alpha);

	float TeeAnimScale, TeeBaseSize;
	CRenderTools::GetRenderTeeAnimScaleAndBaseSize(&RenderInfo, TeeAnimScale, TeeBaseSize);
	vec2 BodyPos = Position + vec2(State.GetBody()->m_X, State.GetBody()->m_Y) * TeeAnimScale;
	if(RenderInfo.m_TeeRenderFlags & TEE_EFFECT_FROZEN)
	{
		GameClient()->m_Effects.FreezingFlakes(BodyPos, vec2(32, 32), Alpha);
	}

	int QuadOffsetToEmoticon = NUM_WEAPONS * 2 + 2 + 2;
	if((Player.m_PlayerFlags & PLAYERFLAG_CHATTING) && !m_pClient->m_aClients[ClientID].m_Afk)
	{
		int CurEmoticon = (SPRITE_DOTDOT - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[CurEmoticon]);
		int QuadOffset = QuadOffsetToEmoticon + CurEmoticon;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(ClientID < 0)
		return;

	if(g_Config.m_ClAfkEmote && m_pClient->m_aClients[ClientID].m_Afk && !(Client()->DummyConnected() && ClientID == m_pClient->m_aLocalIDs[!g_Config.m_ClDummy]))
	{
		int CurEmoticon = (SPRITE_ZZZ - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[CurEmoticon]);
		int QuadOffset = QuadOffsetToEmoticon + CurEmoticon;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClShowEmotes && !m_pClient->m_aClients[ClientID].m_EmoticonIgnore && m_pClient->m_aClients[ClientID].m_EmoticonStartTick != -1)
	{
		float SinceStart = (Client()->GameTick(g_Config.m_ClDummy) - m_pClient->m_aClients[ClientID].m_EmoticonStartTick) + (Client()->IntraGameTickSincePrev(g_Config.m_ClDummy) - m_pClient->m_aClients[ClientID].m_EmoticonStartFraction);
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
			int QuadOffset = QuadOffsetToEmoticon + m_pClient->m_aClients[ClientID].m_Emoticon;
			Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[m_pClient->m_aClients[ClientID].m_Emoticon]);
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x, Position.y - 23.f - 32.f * h, 1.f, (64.f * h) / 64.f);

			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			Graphics()->QuadsSetRotation(0);
		}
	}
}

inline bool CPlayers::IsPlayerInfoAvailable(int ClientID) const
{
	const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, ClientID);
	const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, ClientID);
	return pPrevInfo && pInfo;
}

void CPlayers::OnRender()
{
	CTeeRenderInfo aRenderInfo[MAX_CLIENTS];

	// update RenderInfo for ninja
	bool IsTeamplay = false;
	if(m_pClient->m_Snap.m_pGameInfoObj)
		IsTeamplay = (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS) != 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		aRenderInfo[i] = m_pClient->m_aClients[i].m_RenderInfo;
		aRenderInfo[i].m_TeeRenderFlags = 0;
		if(m_pClient->m_aClients[i].m_FreezeEnd != 0)
			aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
		if(m_pClient->m_aClients[i].m_LiveFrozen)
			aRenderInfo[i].m_TeeRenderFlags |= TEE_EFFECT_FROZEN;

		const CGameClient::CSnapState::CCharacterInfo &CharacterInfo = m_pClient->m_Snap.m_aCharacters[i];
		const bool Frozen = CharacterInfo.m_HasExtendedData && CharacterInfo.m_ExtendedData.m_FreezeEnd != 0;
		if((CharacterInfo.m_Cur.m_Weapon == WEAPON_NINJA || (Frozen && !m_pClient->m_GameInfo.m_NoSkinChangeForFrozen)) && g_Config.m_ClShowNinja)
		{
			// change the skin for the player to the ninja
			const auto *pSkin = m_pClient->m_Skins.FindOrNullptr("x_ninja");
			if(pSkin != nullptr)
			{
				aRenderInfo[i].m_OriginalRenderSkin = pSkin->m_OriginalSkin;
				aRenderInfo[i].m_ColorableRenderSkin = pSkin->m_ColorableSkin;
				aRenderInfo[i].m_BloodColor = pSkin->m_BloodColor;
				aRenderInfo[i].m_SkinMetrics = pSkin->m_Metrics;
				aRenderInfo[i].m_CustomColoredSkin = IsTeamplay;
				if(!IsTeamplay)
				{
					aRenderInfo[i].m_ColorBody = ColorRGBA(1, 1, 1);
					aRenderInfo[i].m_ColorFeet = ColorRGBA(1, 1, 1);
				}
			}
		}
	}
	const CSkin *pSkin = m_pClient->m_Skins.Find("x_spec");
	CTeeRenderInfo RenderInfoSpec;
	RenderInfoSpec.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	RenderInfoSpec.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	RenderInfoSpec.m_BloodColor = pSkin->m_BloodColor;
	RenderInfoSpec.m_SkinMetrics = pSkin->m_Metrics;
	RenderInfoSpec.m_CustomColoredSkin = false;
	RenderInfoSpec.m_Size = 64.0f;
	const int LocalClientID = m_pClient->m_Snap.m_LocalClientID;

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
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(ClientID == LocalClientID || !m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !IsPlayerInfoAvailable(ClientID))
		{
			continue;
		}
		RenderHook(&m_pClient->m_aClients[ClientID].m_RenderPrev, &m_pClient->m_aClients[ClientID].m_RenderCur, &aRenderInfo[ClientID], ClientID);
	}
	if(LocalClientID != -1 && m_pClient->m_Snap.m_aCharacters[LocalClientID].m_Active && IsPlayerInfoAvailable(LocalClientID))
	{
		const CGameClient::CClientData *pLocalClientData = &m_pClient->m_aClients[LocalClientID];
		RenderHook(&pLocalClientData->m_RenderPrev, &pLocalClientData->m_RenderCur, &aRenderInfo[LocalClientID], LocalClientID);
	}

	// render spectating players
	for(auto &m_aClient : m_pClient->m_aClients)
	{
		if(!m_aClient.m_SpecCharPresent)
		{
			continue;
		}
		RenderTools()->RenderTee(CAnimState::GetIdle(), &RenderInfoSpec, EMOTE_BLINK, vec2(1, 0), m_aClient.m_SpecChar);
	}

	// render everyone else's tee, then either our own or the tee we are spectating.
	const int RenderLastID = (m_pClient->m_Snap.m_SpecInfo.m_SpectatorID != SPEC_FREEVIEW && m_pClient->m_Snap.m_SpecInfo.m_Active) ? m_pClient->m_Snap.m_SpecInfo.m_SpectatorID : LocalClientID;

	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(ClientID == RenderLastID || !m_pClient->m_Snap.m_aCharacters[ClientID].m_Active || !IsPlayerInfoAvailable(ClientID))
		{
			continue;
		}

		RenderHookCollLine(&m_pClient->m_aClients[ClientID].m_RenderPrev, &m_pClient->m_aClients[ClientID].m_RenderCur, ClientID);

		// don't render offscreen
		vec2 *pRenderPos = &m_pClient->m_aClients[ClientID].m_RenderPos;
		if(pRenderPos->x < ScreenX0 || pRenderPos->x > ScreenX1 || pRenderPos->y < ScreenY0 || pRenderPos->y > ScreenY1)
		{
			continue;
		}
		RenderPlayer(&m_pClient->m_aClients[ClientID].m_RenderPrev, &m_pClient->m_aClients[ClientID].m_RenderCur, &aRenderInfo[ClientID], ClientID);
	}
	if(RenderLastID != -1 && m_pClient->m_Snap.m_aCharacters[RenderLastID].m_Active && IsPlayerInfoAvailable(RenderLastID))
	{
		const CGameClient::CClientData *pClientData = &m_pClient->m_aClients[RenderLastID];
		RenderHookCollLine(&pClientData->m_RenderPrev, &pClientData->m_RenderCur, RenderLastID);
		RenderPlayer(&pClientData->m_RenderPrev, &pClientData->m_RenderCur, &aRenderInfo[RenderLastID], RenderLastID);
	}
}

void CPlayers::OnInit()
{
	m_WeaponEmoteQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		float ScaleX, ScaleY;
		RenderTools()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY);
		Graphics()->QuadsSetSubset(0, 1, 1, 0);
		RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY);
	}
	float ScaleX, ScaleY;

	// at the end the hand
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 20.f);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, -12.f, -8.f, 24.f, 16.f);

	for(int i = 0; i < NUM_EMOTICONS; ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->QuadContainerAddSprite(m_WeaponEmoteQuadContainerIndex, 64.f);
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
					RenderTools()->GetSpriteScaleImpl(96, 64, ScaleX, ScaleY);
				}
				else
					RenderTools()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_aSpriteMuzzles[n], ScaleX, ScaleY);
			}

			float SWidth = (g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX) * (4.0f / 3.0f);
			float SHeight = g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY;

			Graphics()->QuadsSetSubset(0, 0, 1, 1);
			if(WEAPON_NINJA == i)
				RenderTools()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], 160.f * ScaleX, 160.f * ScaleY);
			else
				RenderTools()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], SWidth, SHeight);

			Graphics()->QuadsSetSubset(0, 1, 1, 0);
			if(WEAPON_NINJA == i)
				RenderTools()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], 160.f * ScaleX, 160.f * ScaleY);
			else
				RenderTools()->QuadContainerAddSprite(m_aWeaponSpriteMuzzleQuadContainerIndex[i], SWidth, SHeight);
		}
		Graphics()->QuadContainerUpload(m_aWeaponSpriteMuzzleQuadContainerIndex[i]);
	}

	Graphics()->QuadsSetSubset(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0.f);
}
