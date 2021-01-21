/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <game/generated/client_data.h>
#include <game/generated/client_data7.h>
#include <game/generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/extrainfo.h>

#include <game/client/components/effects.h>
#include <game/client/components/flow.h>

#include "items.h"

void CItems::OnReset()
{
	m_NumExtraProjectiles = 0;
}

void CItems::RenderProjectile(const CNetObj_Projectile *pCurrent, int ItemID)
{
	int CurWeapon = clamp(pCurrent->m_Type, 0, NUM_WEAPONS - 1);

	// get positions
	float Curvature = 0;
	float Speed = 0;
	if(CurWeapon == WEAPON_GRENADE)
	{
		Curvature = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GrenadeCurvature;
		Speed = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GrenadeSpeed;
	}
	else if(CurWeapon == WEAPON_SHOTGUN)
	{
		Curvature = m_pClient->m_Tuning[g_Config.m_ClDummy].m_ShotgunCurvature;
		Speed = m_pClient->m_Tuning[g_Config.m_ClDummy].m_ShotgunSpeed;
	}
	else if(CurWeapon == WEAPON_GUN)
	{
		Curvature = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GunCurvature;
		Speed = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GunSpeed;
	}

	bool LocalPlayerInGame = false;

	if(m_pClient->m_Snap.m_pLocalInfo)
		LocalPlayerInGame = m_pClient->m_aClients[m_pClient->m_Snap.m_pLocalInfo->m_ClientID].m_Team != -1;

	static float s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);
	if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);

	float Ct;
	if(m_pClient->Predict() && m_pClient->AntiPingGrenade() && LocalPlayerInGame && !(Client()->State() == IClient::STATE_DEMOPLAYBACK))
		Ct = ((float)(Client()->PredGameTick(g_Config.m_ClDummy) - 1 - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy)) / (float)SERVER_TICK_SPEED;
	else
		Ct = (Client()->PrevGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) / (float)SERVER_TICK_SPEED + s_LastGameTickTime;
	if(Ct < 0)
		return; // projectile haven't been shot yet

	vec2 StartPos;
	vec2 StartVel;

	ExtractInfo(pCurrent, &StartPos, &StartVel);

	vec2 Pos = CalcPos(StartPos, StartVel, Curvature, Speed, Ct);
	vec2 PrevPos = CalcPos(StartPos, StartVel, Curvature, Speed, Ct - 0.001f);

	float Alpha = 1.f;
	if(UseExtraInfo(pCurrent))
	{
		int Owner;
		ExtractExtraInfo(pCurrent, &Owner, 0, 0, 0);
		if(Owner >= 0 && m_pClient->IsOtherTeam(Owner))
			Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;
	}

	vec2 Vel = Pos - PrevPos;

	// add particle for this projectile
	// don't check for validity of the projectile for the current weapon here, so particle effects are rendered for mod compability
	if(CurWeapon == WEAPON_GRENADE)
	{
		m_pClient->m_pEffects->SmokeTrail(Pos, Vel * -1, Alpha);
		static float s_Time = 0.0f;
		static float s_LastLocalTime = LocalTime();

		if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		{
			const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
			if(!pInfo->m_Paused)
				s_Time += (LocalTime() - s_LastLocalTime) * pInfo->m_Speed;
		}
		else
		{
			if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
				s_Time += LocalTime() - s_LastLocalTime;
		}

		Graphics()->QuadsSetRotation(s_Time * pi * 2 * 2 + ItemID);
		s_LastLocalTime = LocalTime();
	}
	else
	{
		m_pClient->m_pEffects->BulletTrail(Pos, Alpha);

		if(length(Vel) > 0.00001f)
			Graphics()->QuadsSetRotation(angle(Vel));
		else
			Graphics()->QuadsSetRotation(0);
	}

	if(GameClient()->m_GameSkin.m_SpriteWeaponProjectiles[CurWeapon] != -1)
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteWeaponProjectiles[CurWeapon]);
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);

		int QuadOffset = 2 + 8 + NUM_WEAPONS + CurWeapon;

		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y);
	}
}

void CItems::RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted)
{
	const int c[] = {
		SPRITE_PICKUP_HEALTH,
		SPRITE_PICKUP_ARMOR,
		SPRITE_PICKUP_WEAPON,
		SPRITE_PICKUP_NINJA};

	int CurWeapon = clamp(pCurrent->m_Subtype, 0, NUM_WEAPONS - 1);

	if(c[pCurrent->m_Type] == SPRITE_PICKUP_HEALTH)
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupHealth);
	else if(c[pCurrent->m_Type] == SPRITE_PICKUP_ARMOR)
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupArmor);
	else if(c[pCurrent->m_Type] == SPRITE_PICKUP_WEAPON)
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupWeapons[CurWeapon]);
	}
	else if(c[pCurrent->m_Type] == SPRITE_PICKUP_NINJA)
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupNinja);
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	int QuadOffset = 2;

	float IntraTick = IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), IntraTick);
	float Angle = 0.0f;
	if(pCurrent->m_Type == POWERUP_WEAPON)
	{
		Angle = 0; //-pi/6;//-0.25f * pi * 2.0f;
		QuadOffset += 2 + CurWeapon;
	}
	else
	{
		QuadOffset += pCurrent->m_Type;

		if(c[pCurrent->m_Type] == SPRITE_PICKUP_NINJA)
		{
			QuadOffset = 2 + 8 - 1; // ninja is the last weapon
			m_pClient->m_pEffects->PowerupShine(Pos, vec2(96, 18));
			Pos.x -= 10.0f;
		}
	}

	Graphics()->QuadsSetRotation(Angle);

	static float s_Time = 0.0f;
	static float s_LastLocalTime = LocalTime();
	float Offset = Pos.y / 32.0f + Pos.x / 32.0f;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		const IDemoPlayer::CInfo *pInfo = DemoPlayer()->BaseInfo();
		if(!pInfo->m_Paused)
			s_Time += (LocalTime() - s_LastLocalTime) * pInfo->m_Speed;
	}
	else
	{
		if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
			s_Time += LocalTime() - s_LastLocalTime;
	}
	Pos.x += cosf(s_Time * 2.0f + Offset) * 2.5f;
	Pos.y += sinf(s_Time * 2.0f + Offset) * 2.5f;
	s_LastLocalTime = LocalTime();

	Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y);
}

void CItems::RenderFlag(const CNetObj_Flag *pPrev, const CNetObj_Flag *pCurrent, const CNetObj_GameData *pPrevGameData, const CNetObj_GameData *pCurGameData)
{
	float Angle = 0.0f;
	float Size = 42.0f;

	if(pCurrent->m_Team == TEAM_RED)
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);
	else
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
	int QuadOffset = 0;

	if(pCurrent->m_Team != TEAM_RED)
		++QuadOffset;

	Graphics()->QuadsSetRotation(Angle);

	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), Client()->IntraGameTick(g_Config.m_ClDummy));

	if(pCurGameData)
	{
		int FlagCarrier = (pCurrent->m_Team == TEAM_RED) ? pCurGameData->m_FlagCarrierRed : pCurGameData->m_FlagCarrierBlue;
		// use the flagcarriers position if available
		if(FlagCarrier >= 0 && m_pClient->m_Snap.m_aCharacters[FlagCarrier].m_Active)
			Pos = m_pClient->m_aClients[FlagCarrier].m_RenderPos;

		// make sure that the flag isn't interpolated between capture and return
		if(pPrevGameData &&
			((pCurrent->m_Team == TEAM_RED && pPrevGameData->m_FlagCarrierRed != pCurGameData->m_FlagCarrierRed) ||
				(pCurrent->m_Team == TEAM_BLUE && pPrevGameData->m_FlagCarrierBlue != pCurGameData->m_FlagCarrierBlue)))
			Pos = vec2(pCurrent->m_X, pCurrent->m_Y);
	}

	Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y - Size * 0.75f);
}

void CItems::RenderLaser(const struct CNetObj_Laser *pCurrent, bool IsPredicted)
{
	ColorRGBA RGB;
	vec2 Pos = vec2(pCurrent->m_X, pCurrent->m_Y);
	vec2 From = vec2(pCurrent->m_FromX, pCurrent->m_FromY);
	float Len = distance(Pos, From);
	RGB = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClLaserOutlineColor));
	ColorRGBA OuterColor(RGB.r, RGB.g, RGB.b, 1.0f);
	RGB = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClLaserInnerColor));
	ColorRGBA InnerColor(RGB.r, RGB.g, RGB.b, 1.0f);

	vec2 Dir;
	if(Len > 0)
	{
		Dir = normalize_pre_length(Pos - From, Len);

		float Ticks;
		if(IsPredicted)
			Ticks = (float)(Client()->PredGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy);
		else
			Ticks = (float)(Client()->GameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) + Client()->IntraGameTick(g_Config.m_ClDummy);
		float Ms = (Ticks / 50.0f) * 1000.0f;
		float a = Ms / m_pClient->m_Tuning[g_Config.m_ClDummy].m_LaserBounceDelay;
		a = clamp(a, 0.0f, 1.0f);
		float Ia = 1 - a;

		vec2 Out;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();

		// do outline
		Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
		Out = vec2(Dir.y, -Dir.x) * (7.0f * Ia);

		IGraphics::CFreeformItem Freeform(
			From.x - Out.x, From.y - Out.y,
			From.x + Out.x, From.y + Out.y,
			Pos.x - Out.x, Pos.y - Out.y,
			Pos.x + Out.x, Pos.y + Out.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		// do inner
		Out = vec2(Dir.y, -Dir.x) * (5.0f * Ia);
		Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f); // center

		Freeform = IGraphics::CFreeformItem(
			From.x - Out.x, From.y - Out.y,
			From.x + Out.x, From.y + Out.y,
			Pos.x - Out.x, Pos.y - Out.y,
			Pos.x + Out.x, Pos.y + Out.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		Graphics()->QuadsEnd();
	}

	// render head
	{
		int CurParticle = (Client()->GameTick(g_Config.m_ClDummy) % 3);
		int QuadOffset = 2 + 8 + NUM_WEAPONS * 2 + CurParticle;

		Graphics()->TextureSet(GameClient()->m_ParticlesSkin.m_SpriteParticleSplat[CurParticle]);
		Graphics()->QuadsSetRotation(Client()->GameTick(g_Config.m_ClDummy));
		Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y);
		Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y, 20.f / 24.f, 20.f / 24.f);
	}
}

void CItems::OnRender()
{
	if(Client()->State() < IClient::STATE_ONLINE)
		return;

	bool UsePredicted = GameClient()->Predict() && GameClient()->AntiPingGunfire();
	if(UsePredicted)
	{
		for(auto *pProj = (CProjectile *)GameClient()->m_PredictedWorld.FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile *)pProj->NextEntity())
		{
			CNetObj_Projectile Data;
			if(pProj->m_Type != WEAPON_SHOTGUN || pProj->m_Explosive || pProj->m_Freeze)
				pProj->FillExtraInfo(&Data);
			else
				pProj->FillInfo(&Data);
			RenderProjectile(&Data, pProj->ID());
		}
		for(auto *pLaser = (CLaser *)GameClient()->m_PredictedWorld.FindFirst(CGameWorld::ENTTYPE_LASER); pLaser; pLaser = (CLaser *)pLaser->NextEntity())
		{
			if(pLaser->GetOwner() < 0 || (pLaser->GetOwner() >= 0 && !GameClient()->m_aClients[pLaser->GetOwner()].m_IsPredictedLocal))
				continue;
			CNetObj_Laser Data;
			pLaser->FillInfo(&Data);
			RenderLaser(&Data, true);
		}
		for(auto *pPickup = (CPickup *)GameClient()->m_PredictedWorld.FindFirst(CGameWorld::ENTTYPE_PICKUP); pPickup; pPickup = (CPickup *)pPickup->NextEntity())
		{
			if(auto *pPrev = (CPickup *)GameClient()->m_PrevPredictedWorld.GetEntity(pPickup->ID(), CGameWorld::ENTTYPE_PICKUP))
			{
				CNetObj_Pickup Data, Prev;
				pPickup->FillInfo(&Data);
				pPrev->FillInfo(&Prev);
				RenderPickup(&Prev, &Data, true);
			}
		}
	}

	int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

		if(Item.m_Type == NETOBJTYPE_PROJECTILE)
		{
			if(UsePredicted)
			{
				if(auto *pProj = (CProjectile *)GameClient()->m_GameWorld.FindMatch(Item.m_ID, Item.m_Type, pData))
				{
					if(pProj->m_LastRenderTick <= 0 && (pProj->m_Type != WEAPON_SHOTGUN || (!pProj->m_Freeze && !pProj->m_Explosive)) // skip ddrace shotgun bullets
						&& (pProj->m_Type == WEAPON_SHOTGUN || fabs(length(pProj->m_Direction) - 1.f) < 0.02) // workaround to skip grenades on ball mod
						&& (pProj->GetOwner() < 0 || !GameClient()->m_aClients[pProj->GetOwner()].m_IsPredictedLocal) // skip locally predicted projectiles
						&& !Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID))
					{
						ReconstructSmokeTrail((const CNetObj_Projectile *)pData, Item.m_ID, pProj->m_DestroyTick);
					}
					pProj->m_LastRenderTick = Client()->GameTick(g_Config.m_ClDummy);
					continue;
				}
			}
			RenderProjectile((const CNetObj_Projectile *)pData, Item.m_ID);
		}
		else if(Item.m_Type == NETOBJTYPE_PICKUP)
		{
			if(UsePredicted)
				continue;
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID);
			if(pPrev)
				RenderPickup((const CNetObj_Pickup *)pPrev, (const CNetObj_Pickup *)pData);
		}
		else if(Item.m_Type == NETOBJTYPE_LASER)
		{
			if(UsePredicted)
			{
				auto *pLaser = (CLaser *)GameClient()->m_GameWorld.FindMatch(Item.m_ID, Item.m_Type, pData);
				if(pLaser && pLaser->GetOwner() >= 0 && GameClient()->m_aClients[pLaser->GetOwner()].m_IsPredictedLocal)
					continue;
			}
			RenderLaser((const CNetObj_Laser *)pData);
		}
	}

	// render flag
	for(int i = 0; i < Num; i++)
	{
		IClient::CSnapItem Item;
		const void *pData = Client()->SnapGetItem(IClient::SNAP_CURRENT, i, &Item);

		if(Item.m_Type == NETOBJTYPE_FLAG)
		{
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_ID);
			if(pPrev)
			{
				const void *pPrevGameData = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_GAMEDATA, m_pClient->m_Snap.m_GameDataSnapID);
				RenderFlag(static_cast<const CNetObj_Flag *>(pPrev), static_cast<const CNetObj_Flag *>(pData),
					static_cast<const CNetObj_GameData *>(pPrevGameData), m_pClient->m_Snap.m_pGameDataObj);
			}
		}
	}

	// render extra projectiles
	for(int i = 0; i < m_NumExtraProjectiles; i++)
	{
		if(m_aExtraProjectiles[i].m_StartTick < Client()->GameTick(g_Config.m_ClDummy))
		{
			m_aExtraProjectiles[i] = m_aExtraProjectiles[m_NumExtraProjectiles - 1];
			m_NumExtraProjectiles--;
		}
		else if(!UsePredicted)
			RenderProjectile(&m_aExtraProjectiles[i], 0);
	}

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
}

void CItems::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	m_ItemsQuadContainerIndex = Graphics()->CreateQuadContainer();

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, -21.f, -42.f, 42.f, 84.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, -21.f, -42.f, 42.f, 84.f);

	float ScaleX, ScaleY;
	RenderTools()->GetSpriteScale(SPRITE_PICKUP_HEALTH, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	RenderTools()->GetSpriteScale(SPRITE_PICKUP_ARMOR, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	RenderTools()->GetSpriteScale(&client_data7::g_pData->m_aSprites[client_data7::SPRITE_PICKUP_HAMMER], ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, client_data7::g_pData->m_Weapons.m_aId[WEAPON_HAMMER].m_VisualSize * ScaleX, client_data7::g_pData->m_Weapons.m_aId[WEAPON_HAMMER].m_VisualSize * ScaleY);
	RenderTools()->GetSpriteScale(&client_data7::g_pData->m_aSprites[client_data7::SPRITE_PICKUP_GUN], ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, client_data7::g_pData->m_Weapons.m_aId[WEAPON_GUN].m_VisualSize * ScaleX, client_data7::g_pData->m_Weapons.m_aId[WEAPON_GUN].m_VisualSize * ScaleY);
	RenderTools()->GetSpriteScale(&client_data7::g_pData->m_aSprites[client_data7::SPRITE_PICKUP_SHOTGUN], ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, client_data7::g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_VisualSize * ScaleX, client_data7::g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_VisualSize * ScaleY);
	RenderTools()->GetSpriteScale(&client_data7::g_pData->m_aSprites[client_data7::SPRITE_PICKUP_GRENADE], ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, client_data7::g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_VisualSize * ScaleX, client_data7::g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_VisualSize * ScaleY);
	RenderTools()->GetSpriteScale(&client_data7::g_pData->m_aSprites[client_data7::SPRITE_PICKUP_LASER], ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, client_data7::g_pData->m_Weapons.m_aId[WEAPON_LASER].m_VisualSize * ScaleX, client_data7::g_pData->m_Weapons.m_aId[WEAPON_LASER].m_VisualSize * ScaleY);
	RenderTools()->GetSpriteScale(SPRITE_PICKUP_NINJA, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 128.f * ScaleX, 128.f * ScaleY);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		RenderTools()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY);
	}

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 32.f);
	}

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 24.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 24.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 24.f);
}

void CItems::AddExtraProjectile(CNetObj_Projectile *pProj)
{
	if(m_NumExtraProjectiles != MAX_EXTRA_PROJECTILES)
	{
		m_aExtraProjectiles[m_NumExtraProjectiles] = *pProj;
		m_NumExtraProjectiles++;
	}
}

void CItems::ReconstructSmokeTrail(const CNetObj_Projectile *pCurrent, int ItemID, int DestroyTick)
{
	bool LocalPlayerInGame = false;

	if(m_pClient->m_Snap.m_pLocalInfo)
		LocalPlayerInGame = m_pClient->m_aClients[m_pClient->m_Snap.m_pLocalInfo->m_ClientID].m_Team != -1;
	if(!m_pClient->AntiPingGunfire() || !LocalPlayerInGame)
		return;
	if(Client()->PredGameTick(g_Config.m_ClDummy) == pCurrent->m_StartTick)
		return;

	// get positions
	float Curvature = 0;
	float Speed = 0;
	if(pCurrent->m_Type == WEAPON_GRENADE)
	{
		Curvature = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GrenadeCurvature;
		Speed = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GrenadeSpeed;
	}
	else if(pCurrent->m_Type == WEAPON_SHOTGUN)
	{
		Curvature = m_pClient->m_Tuning[g_Config.m_ClDummy].m_ShotgunCurvature;
		Speed = m_pClient->m_Tuning[g_Config.m_ClDummy].m_ShotgunSpeed;
	}
	else if(pCurrent->m_Type == WEAPON_GUN)
	{
		Curvature = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GunCurvature;
		Speed = m_pClient->m_Tuning[g_Config.m_ClDummy].m_GunSpeed;
	}

	float Pt = ((float)(Client()->PredGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy)) / (float)SERVER_TICK_SPEED;
	if(Pt < 0)
		return; // projectile haven't been shot yet

	float Gt = (Client()->PrevGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) / (float)SERVER_TICK_SPEED + Client()->GameTickTime(g_Config.m_ClDummy);

	vec2 StartPos;
	vec2 StartVel;

	ExtractInfo(pCurrent, &StartPos, &StartVel);

	float Alpha = 1.f;
	if(UseExtraInfo(pCurrent))
	{
		int Owner;
		ExtractExtraInfo(pCurrent, &Owner, 0, 0, 0);
		if(Owner >= 0 && m_pClient->IsOtherTeam(Owner))
			Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;
	}

	float T = Pt;
	if(DestroyTick >= 0)
		T = minimum(Pt, ((float)(DestroyTick - 1 - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy)) / (float)SERVER_TICK_SPEED);

	float MinTrailSpan = 0.4f * ((pCurrent->m_Type == WEAPON_GRENADE) ? 0.5f : 0.25f);
	float Step = maximum(Client()->FrameTimeAvg(), (pCurrent->m_Type == WEAPON_GRENADE) ? 0.02f : 0.01f);
	for(int i = 1 + (int)(Gt / Step); i < (int)(T / Step); i++)
	{
		float t = Step * (float)i + 0.4f * Step * (frandom() - 0.5f);
		vec2 Pos = CalcPos(StartPos, StartVel, Curvature, Speed, t);
		vec2 PrevPos = CalcPos(StartPos, StartVel, Curvature, Speed, t - 0.001f);
		vec2 Vel = Pos - PrevPos;
		float TimePassed = Pt - t;
		if(Pt - MinTrailSpan > 0.01f)
			TimePassed = minimum(TimePassed, (TimePassed - MinTrailSpan) / (Pt - MinTrailSpan) * (MinTrailSpan * 0.5f) + MinTrailSpan);
		// add particle for this projectile
		if(pCurrent->m_Type == WEAPON_GRENADE)
			m_pClient->m_pEffects->SmokeTrail(Pos, Vel * -1, Alpha, TimePassed);
		else
			m_pClient->m_pEffects->BulletTrail(Pos, Alpha, TimePassed);
	}
}
