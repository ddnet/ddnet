/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/sorted_array.h>

#include <engine/demo.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/gamecore.h> // get_angle

#include <game/client/components/controls.h>
#include <game/client/components/effects.h>
#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>

#include <engine/textrender.h>

#include "players.h"

void CPlayers::RenderHand(CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset, float Alpha)
{
	vec2 HandPos = CenterPos + Dir;
	float Angle = GetAngle(Dir);
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

inline float NormalizeAngular(float f)
{
	return fmod(f + pi * 2, pi * 2);
}

inline float AngularMixDirection(float Src, float Dst) { return sinf(Dst - Src) > 0 ? 1 : -1; }
inline float AngularDistance(float Src, float Dst) { return asinf(sinf(Dst - Src)); }

inline float AngularApproach(float Src, float Dst, float Amount)
{
	float d = AngularMixDirection(Src, Dst);
	float n = Src + Amount * d;
	if(AngularMixDirection(n, Dst) != d)
		return Dst;
	return n;
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
		Graphics()->QuadsSetRotation(GetAngle(Dir) + pi);
		// render head
		int QuadOffset = NUM_WEAPONS * 2 + 2;
		if(OtherTeam)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, g_Config.m_ClShowOthersAlpha / 100.0f);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookPos.x, HookPos.y);

		// render chain
		++QuadOffset;
		static IGraphics::SRenderSpriteInfo s_HookChainRenderInfo[1024];
		int HookChainCount = 0;
		for(float f = 24; f < d && HookChainCount < 1024; f += 24, ++HookChainCount)
		{
			vec2 p = HookPos + Dir * f;
			s_HookChainRenderInfo[HookChainCount].m_Pos[0] = p.x;
			s_HookChainRenderInfo[HookChainCount].m_Pos[1] = p.y;

			s_HookChainRenderInfo[HookChainCount].m_Scale = 1;
			s_HookChainRenderInfo[HookChainCount].m_Rotation = GetAngle(Dir) + pi;
		}
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHookChain);
		Graphics()->RenderQuadContainerAsSpriteMultiple(m_WeaponEmoteQuadContainerIndex, QuadOffset, HookChainCount, s_HookChainRenderInfo);

		Graphics()->QuadsSetRotation(0);
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

		RenderHand(&RenderInfo, Position, normalize(HookPos - Pos), -pi / 2, vec2(20, 0), OtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f);
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
	float Alpha = OtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;

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
	float AttackTime = (Client()->PrevGameTick(g_Config.m_ClDummy) - Player.m_AttackTick) / (float)SERVER_TICK_SPEED + Client()->GameTickTime(g_Config.m_ClDummy);
	float LastAttackTime = (Client()->PrevGameTick(g_Config.m_ClDummy) - Player.m_AttackTick) / (float)SERVER_TICK_SPEED + s_LastGameTickTime;
	if(ClientID >= 0 && m_pClient->m_aClients[ClientID].m_IsPredictedLocal && m_pClient->AntiPingGunfire())
	{
		PredictLocalWeapons = true;
		AttackTime = (Client()->PredIntraGameTick(g_Config.m_ClDummy) + (Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Player.m_AttackTick)) / (float)SERVER_TICK_SPEED;
		LastAttackTime = (s_LastPredIntraTick + (Client()->PredGameTick(g_Config.m_ClDummy) - 1 - Player.m_AttackTick)) / (float)SERVER_TICK_SPEED;
	}
	float AttackTicksPassed = AttackTime * SERVER_TICK_SPEED;

	float Angle;
	if(Local && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's the local player we are rendering
		Angle = GetAngle(m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy]);
	}
	else
	{
		float AngleIntraTick = IntraTick;
		// using unpredicted angle when rendering other players in-game
		if(ClientID >= 0)
			AngleIntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
		// If the player moves their weapon through top, then change
		// the end angle by 2*Pi, so that the mix function will use the
		// short path and not the long one.
		if(Player.m_Angle > (256.0f * pi) && Prev.m_Angle < 0)
			Player.m_Angle -= 256.0f * 2 * pi;
		else if(Player.m_Angle < 0 && Prev.m_Angle > (256.0f * pi))
			Player.m_Angle += 256.0f * 2 * pi;

		Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, AngleIntraTick) / 256.0f;
	}

	vec2 Direction = GetDirection((int)(Angle * 256.0f));
	vec2 Position;
	if(in_range(ClientID, MAX_CLIENTS - 1))
		Position = m_pClient->m_aClients[ClientID].m_RenderPos;
	else
		Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	vec2 Vel = mix(vec2(Prev.m_VelX / 256.0f, Prev.m_VelY / 256.0f), vec2(Player.m_VelX / 256.0f, Player.m_VelY / 256.0f), IntraTick);

	m_pClient->m_pFlow->Add(Position, Vel * 100.0f, 10.0f);

	RenderInfo.m_GotAirJump = Player.m_Jumped & 2 ? 0 : 1;

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y + 16);
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);

	// evaluate animation
	float WalkTime = fmod(absolute(Position.x), 100.0f) / 100.0f;
	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if(InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f); // TODO: some sort of time here
	else if(Stationary)
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f); // TODO: some sort of time here
	else if(!WantOtherDir)
		State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);

	if(Player.m_Weapon == WEAPON_HAMMER)
		State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], clamp(LastAttackTime * 5.0f, 0.0f, 1.0f), 1.0f);
	if(Player.m_Weapon == WEAPON_NINJA)
		State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], clamp(LastAttackTime * 2.0f, 0.0f, 1.0f), 1.0f);

	// do skidding
	if(!InAir && WantOtherDir && length(Vel * 50) > 500.0f)
	{
		static int64 SkidSoundTime = 0;
		if(time() - SkidSoundTime > time_freq() / 10)
		{
			if(g_Config.m_SndGame)
				m_pClient->m_pSounds->PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SKID, 0.25f, Position);
			SkidSoundTime = time();
		}

		m_pClient->m_pEffects->SkidTrail(
			Position + vec2(-Player.m_Direction * 6, 12),
			vec2(-Player.m_Direction * 100 * length(Vel), -50));
	}

	// draw gun
	{
		bool AlwaysRenderHookColl = GameClient()->m_GameInfo.m_AllowHookColl && (Local ? g_Config.m_ClShowHookCollOwn : g_Config.m_ClShowHookCollOther) == 2;
		bool RenderHookCollPlayer = ClientID >= 0 && Player.m_PlayerFlags & PLAYERFLAG_AIM && (Local ? g_Config.m_ClShowHookCollOwn : g_Config.m_ClShowHookCollOther) > 0;
		bool RenderHookCollVideo = true;
#if defined(CONF_VIDEORECORDER)
		RenderHookCollVideo = !IVideo::Current() || g_Config.m_ClVideoShowHookCollOther || Local;
#endif
		if((AlwaysRenderHookColl || RenderHookCollPlayer) && RenderHookCollVideo)
		{
			vec2 ExDirection = Direction;

			if(Local && Client()->State() != IClient::STATE_DEMOPLAYBACK)
				ExDirection = normalize(vec2(m_pClient->m_pControls->m_InputData[g_Config.m_ClDummy].m_TargetX, m_pClient->m_pControls->m_InputData[g_Config.m_ClDummy].m_TargetY));

			Graphics()->TextureClear();
			vec2 InitPos = Position;
			vec2 FinishPos = InitPos + ExDirection * (m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookLength - 42.0f);

			Graphics()->LinesBegin();
			ColorRGBA HookCollColor(1.0f, 0.0f, 0.0f);

			float PhysSize = 28.0f;

			vec2 OldPos = InitPos + ExDirection * PhysSize * 1.5f;
			vec2 NewPos = OldPos;

			bool DoBreak = false;
			int Hit = 0;

			do
			{
				OldPos = NewPos;
				NewPos = OldPos + ExDirection * m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookFireSpeed;

				if(distance(InitPos, NewPos) > m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookLength)
				{
					NewPos = InitPos + normalize(NewPos - InitPos) * m_pClient->m_Tuning[g_Config.m_ClDummy].m_HookLength;
					DoBreak = true;
				}

				int TeleNr = 0;
				Hit = Collision()->IntersectLineTeleHook(OldPos, NewPos, &FinishPos, 0x0, &TeleNr);

				if(!DoBreak && Hit)
				{
					if(Hit != TILE_NOHOOK)
					{
						HookCollColor = ColorRGBA(130.0f / 255.0f, 232.0f / 255.0f, 160.0f / 255.0f);
					}
				}

				if(m_pClient->IntersectCharacter(OldPos, FinishPos, FinishPos, ClientID) != -1)
				{
					HookCollColor = ColorRGBA(1.0f, 1.0f, 0.0f);
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
			IGraphics::CLineItem LineItem(InitPos.x, InitPos.y, FinishPos.x, FinishPos.y);
			Graphics()->LinesDraw(&LineItem, 1);
			Graphics()->LinesEnd();
		}

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle * pi * 2 + Angle);

		if(ClientID < 0)
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);

		// normal weapons
		int iw = clamp(Player.m_Weapon, 0, NUM_WEAPONS - 1);
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeapons[iw]);
		int QuadOffset = iw * 2 + (Direction.x < 0 ? 1 : 0);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);

		vec2 Dir = Direction;
		float Recoil = 0.0f;
		vec2 p;
		if(Player.m_Weapon == WEAPON_HAMMER)
		{
			// Static position for hammer
			p = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
			// if attack is under way, bash stuffs
			if(Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2);
				p.x -= g_pData->m_Weapons.m_aId[iw].m_Offsetx;
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2);
			}
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, p.x, p.y);
		}
		else if(Player.m_Weapon == WEAPON_NINJA)
		{
			p = Position;
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;

			if(Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2);
				p.x -= g_pData->m_Weapons.m_aId[iw].m_Offsetx;
				m_pClient->m_pEffects->PowerupShine(p + vec2(32, 0), vec2(32, 12));
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2);
				m_pClient->m_pEffects->PowerupShine(p - vec2(32, 0), vec2(32, 12));
			}
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, p.x, p.y);

			// HADOKEN
			if(AttackTime <= 1 / 6.f && g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles)
			{
				int IteX = rand() % g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles;
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
				if(g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX])
				{
					vec2 Pos1, Pos0;
					vec2 Dir;
					if(PredictLocalWeapons)
						Dir = vec2(pPlayerChar->m_X, pPlayerChar->m_Y) - vec2(pPrevChar->m_X, pPrevChar->m_Y);
					else
						Dir = vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_Y) - vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_Y);
					float HadOkenAngle = 0;
					if(absolute(Dir.x) > 0.0001f || absolute(Dir.y) > 0.0001f)
					{
						Dir = normalize(Dir);
						HadOkenAngle = GetAngle(Dir);
					}
					else
					{
						Dir = vec2(1, 0);
					}
					Graphics()->QuadsSetRotation(HadOkenAngle);
					int QuadOffset = IteX * 2;
					vec2 DirY(-Dir.y, Dir.x);
					p = Position;
					float OffsetX = g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsetx;
					p -= Dir * OffsetX;
					Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponsMuzzles[iw][IteX]);
					Graphics()->RenderQuadContainerAsSprite(m_WeaponSpriteMuzzleQuadContainerIndex[iw], QuadOffset, p.x, p.y);
				}
			}
		}
		else
		{
			// TODO: should be an animation
			Recoil = 0;
			float a = AttackTicksPassed / 5.0f;
			if(a < 1)
				Recoil = sinf(a * pi);
			p = Position + Dir * g_pData->m_Weapons.m_aId[iw].m_Offsetx - Dir * Recoil * 10.0f;
			p.y += g_pData->m_Weapons.m_aId[iw].m_Offsety;
			if(Player.m_Weapon == WEAPON_GUN && g_Config.m_ClOldGunPosition)
				p.y -= 8;
			Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, p.x, p.y);
		}

		if(Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN)
		{
			// check if we're firing stuff
			if(g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles) //prev.attackticks)
			{
				float Alpha = 0.0f;
				if(AttackTicksPassed < g_pData->m_Weapons.m_aId[iw].m_Muzzleduration + 3)
				{
					float t = AttackTicksPassed / g_pData->m_Weapons.m_aId[iw].m_Muzzleduration;
					Alpha = mix(2.0f, 0.0f, minimum(1.0f, maximum(0.0f, t)));
				}

				int IteX = rand() % g_pData->m_Weapons.m_aId[iw].m_NumSpriteMuzzles;
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
				if(Alpha > 0.0f && g_pData->m_Weapons.m_aId[iw].m_aSpriteMuzzles[IteX])
				{
					float OffsetY = -g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsety;
					int QuadOffset = IteX * 2 + (Direction.x < 0 ? 1 : 0);
					if(Direction.x < 0)
						OffsetY = -OffsetY;

					vec2 DirY(-Dir.y, Dir.x);
					vec2 MuzzlePos = p + Dir * g_pData->m_Weapons.m_aId[iw].m_Muzzleoffsetx + DirY * OffsetY;
					Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponsMuzzles[iw][IteX]);
					Graphics()->RenderQuadContainerAsSprite(m_WeaponSpriteMuzzleQuadContainerIndex[iw], QuadOffset, MuzzlePos.x, MuzzlePos.y);
				}
			}
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);

		switch(Player.m_Weapon)
		{
		case WEAPON_GUN: RenderHand(&RenderInfo, p, Direction, -3 * pi / 4, vec2(-15, 4), Alpha); break;
		case WEAPON_SHOTGUN: RenderHand(&RenderInfo, p, Direction, -pi / 2, vec2(-5, 4), Alpha); break;
		case WEAPON_GRENADE: RenderHand(&RenderInfo, p, Direction, -pi / 2, vec2(-4, 7), Alpha); break;
		}
	}

	// render the "shadow" tee
	if(Local && ((g_Config.m_Debug && g_Config.m_ClUnpredictedShadow >= 0) || g_Config.m_ClUnpredictedShadow == 1))
	{
		vec2 GhostPosition = Position;
		if(ClientID >= 0)
			GhostPosition = mix(
				vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Prev.m_Y),
				vec2(m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_X, m_pClient->m_Snap.m_aCharacters[ClientID].m_Cur.m_Y),
				Client()->IntraGameTick(g_Config.m_ClDummy));

		CTeeRenderInfo Ghost = RenderInfo;
		RenderTools()->RenderTee(&State, &Ghost, Player.m_Emote, Direction, GhostPosition, 0.5f); // render ghost
	}

	RenderInfo.m_Size = 64.0f; // force some settings

	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	Graphics()->QuadsSetRotation(0);
#if defined(CONF_VIDEORECORDER)
	if(((!IVideo::Current() && g_Config.m_ClShowDirection) || (IVideo::Current() && g_Config.m_ClVideoShowDirection)) && ClientID >= 0 && (!Local || DemoPlayer()->IsPlaying()))
#else
	if(g_Config.m_ClShowDirection && ClientID >= 0 && (!Local || DemoPlayer()->IsPlaying()))
#endif
	{
		if(Player.m_Direction == -1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			Graphics()->QuadsSetRotation(pi);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, Position.x - 30.f, Position.y - 70.f);
		}
		else if(Player.m_Direction == 1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, Position.x + 30.f, Position.y - 70.f);
		}
		if(Player.m_Jumped & 1)
		{
			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_ARROW].m_Id);
			Graphics()->QuadsSetRotation(pi * 3 / 2);
			Graphics()->RenderQuadContainerAsSprite(m_DirectionQuadContainerIndex, 0, Position.x, Position.y - 70.f);
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(OtherTeam || ClientID < 0)
		RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position, g_Config.m_ClShowOthersAlpha / 100.0f);
	else
		RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position);

	int QuadOffsetToEmoticon = NUM_WEAPONS * 2 + 2 + 2;
	if((Player.m_PlayerFlags & PLAYERFLAG_CHATTING) && !m_pClient->m_aClients[ClientID].m_Afk)
	{
		int CurEmoticon = (SPRITE_DOTDOT - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_SpriteEmoticons[CurEmoticon]);
		int QuadOffset = QuadOffsetToEmoticon + CurEmoticon;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(ClientID < 0)
		return;

	if(g_Config.m_ClAfkEmote && m_pClient->m_aClients[ClientID].m_Afk && !(Client()->DummyConnected() && ClientID == m_pClient->m_LocalIDs[!g_Config.m_ClDummy]))
	{
		int CurEmoticon = (SPRITE_ZZZ - SPRITE_OOP);
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_SpriteEmoticons[CurEmoticon]);
		int QuadOffset = QuadOffsetToEmoticon + CurEmoticon;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x + 24.f, Position.y - 40.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}

	if(g_Config.m_ClShowEmotes && !m_pClient->m_aClients[ClientID].m_EmoticonIgnore && m_pClient->m_aClients[ClientID].m_EmoticonStart != -1 && m_pClient->m_aClients[ClientID].m_EmoticonStart <= Client()->GameTick(g_Config.m_ClDummy) && m_pClient->m_aClients[ClientID].m_EmoticonStart + 2 * Client()->GameTickSpeed() > Client()->GameTick(g_Config.m_ClDummy))
	{
		int SinceStart = Client()->GameTick(g_Config.m_ClDummy) - m_pClient->m_aClients[ClientID].m_EmoticonStart;
		int FromEnd = m_pClient->m_aClients[ClientID].m_EmoticonStart + 2 * Client()->GameTickSpeed() - Client()->GameTick(g_Config.m_ClDummy);

		float a = 1;

		if(FromEnd < Client()->GameTickSpeed() / 5)
			a = FromEnd / (Client()->GameTickSpeed() / 5.0f);

		float h = 1;
		if(SinceStart < Client()->GameTickSpeed() / 10)
			h = SinceStart / (Client()->GameTickSpeed() / 10.0f);

		float Wiggle = 0;
		if(SinceStart < Client()->GameTickSpeed() / 5)
			Wiggle = SinceStart / (Client()->GameTickSpeed() / 5.0f);

		float WiggleAngle = sinf(5 * Wiggle);

		Graphics()->QuadsSetRotation(pi / 6 * WiggleAngle);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, a * Alpha);
		// client_datas::emoticon is an offset from the first emoticon
		int QuadOffset = QuadOffsetToEmoticon + m_pClient->m_aClients[ClientID].m_Emoticon;
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_SpriteEmoticons[m_pClient->m_aClients[ClientID].m_Emoticon]);
		Graphics()->RenderQuadContainerAsSprite(m_WeaponEmoteQuadContainerIndex, QuadOffset, Position.x, Position.y - 23.f - 32.f * h, 1.f, (64.f * h) / 64.f);

		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		Graphics()->QuadsSetRotation(0);
	}
}

void CPlayers::OnRender()
{
	// update RenderInfo for ninja
	bool IsTeamplay = false;
	if(m_pClient->m_Snap.m_pGameInfoObj)
		IsTeamplay = (m_pClient->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_TEAMS) != 0;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_aRenderInfo[i] = m_pClient->m_aClients[i].m_RenderInfo;
		if(m_pClient->m_Snap.m_aCharacters[i].m_Cur.m_Weapon == WEAPON_NINJA && g_Config.m_ClShowNinja)
		{
			// change the skin for the player to the ninja
			int Skin = m_pClient->m_pSkins->Find("x_ninja");
			if(Skin != -1)
			{
				const CSkin *pSkin = m_pClient->m_pSkins->Get(Skin);
				m_aRenderInfo[i].m_OriginalRenderSkin = pSkin->m_OriginalSkin;
				m_aRenderInfo[i].m_ColorableRenderSkin = pSkin->m_ColorableSkin;
				m_aRenderInfo[i].m_BloodColor = pSkin->m_BloodColor;
				m_aRenderInfo[i].m_SkinMetrics = pSkin->m_Metrics;
				m_aRenderInfo[i].m_CustomColoredSkin = IsTeamplay;
				if(!IsTeamplay)
				{
					m_aRenderInfo[i].m_ColorBody = ColorRGBA(1, 1, 1);
					m_aRenderInfo[i].m_ColorFeet = ColorRGBA(1, 1, 1);
				}
			}
		}
	}
	int Skin = m_pClient->m_pSkins->Find("x_spec");
	const CSkin *pSkin = m_pClient->m_pSkins->Get(Skin);
	m_RenderInfoSpec.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	m_RenderInfoSpec.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	m_RenderInfoSpec.m_BloodColor = pSkin->m_BloodColor;
	m_RenderInfoSpec.m_SkinMetrics = pSkin->m_Metrics;
	m_RenderInfoSpec.m_CustomColoredSkin = false;
	m_RenderInfoSpec.m_Size = 64.0f;

	// render other players in three passes, first pass we render spectees,
	// then everyone but us, and finally we render ourselves
	for(int p = 0; p < 6; p++)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			// only render active characters
			if(p % 3 == 0 && !m_pClient->m_aClients[i].m_SpecCharPresent)
				continue;
			if(p % 3 != 0 && !m_pClient->m_Snap.m_aCharacters[i].m_Active)
				continue;

			if(p % 3 == 0)
			{
				if(p < 3)
				{
					continue;
				}
				bool OtherTeam = m_pClient->IsOtherTeam(i);
				float Alpha = OtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.0f;
				vec2 Pos = m_pClient->m_aClients[i].m_SpecChar;
				RenderTools()->RenderTee(CAnimState::GetIdle(), &m_RenderInfoSpec, EMOTE_BLINK, vec2(1, 0), Pos, Alpha);
			}
			else
			{
				const void *pPrevInfo = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_PLAYERINFO, i);
				const void *pInfo = Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);
				if(!pPrevInfo || !pInfo)
				{
					continue;
				}

				bool Local = m_pClient->m_Snap.m_LocalClientID == i;
				if((p % 3) == 1 && Local)
					continue;
				if((p % 3) == 2 && !Local)
					continue;

				CNetObj_Character PrevChar = m_pClient->m_aClients[i].m_RenderPrev;
				CNetObj_Character CurChar = m_pClient->m_aClients[i].m_RenderCur;

				if(p < 3)
				{
					RenderHook(&PrevChar, &CurChar, &m_aRenderInfo[i], i);
				}
				else
				{
					RenderPlayer(&PrevChar, &CurChar, &m_aRenderInfo[i], i);
				}
			}
		}
	}
}

void CPlayers::OnInit()
{
	m_WeaponEmoteQuadContainerIndex = Graphics()->CreateQuadContainer();

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

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		m_WeaponSpriteMuzzleQuadContainerIndex[i] = Graphics()->CreateQuadContainer();
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
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], 160.f * ScaleX, 160.f * ScaleY);
			else
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], SWidth, SHeight);

			Graphics()->QuadsSetSubset(0, 1, 1, 0);
			if(WEAPON_NINJA == i)
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], 160.f * ScaleX, 160.f * ScaleY);
			else
				RenderTools()->QuadContainerAddSprite(m_WeaponSpriteMuzzleQuadContainerIndex[i], SWidth, SHeight);
		}
	}

	Graphics()->QuadsSetSubset(0.f, 0.f, 1.f, 1.f);
	Graphics()->QuadsSetRotation(0.f);
	// the direction
	m_DirectionQuadContainerIndex = Graphics()->CreateQuadContainer();

	IGraphics::CQuadItem QuadItem(0.f, 0.f, 22.f, 22.f);
	Graphics()->QuadContainerAddQuads(m_DirectionQuadContainerIndex, &QuadItem, 1);
}
