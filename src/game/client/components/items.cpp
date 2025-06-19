/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/generated/client_data.h>
#include <game/generated/protocol.h>

#include <game/mapitems.h>

#include <game/client/gameclient.h>
#include <game/client/laser_data.h>
#include <game/client/pickup_data.h>
#include <game/client/projectile_data.h>
#include <game/client/render.h>

#include <game/client/prediction/entities/laser.h>
#include <game/client/prediction/entities/pickup.h>
#include <game/client/prediction/entities/projectile.h>

#include <game/client/components/effects.h>

#include "items.h"

void CItems::RenderProjectile(const CProjectileData *pCurrent, int ItemId)
{
	int CurWeapon = std::clamp(pCurrent->m_Type, 0, NUM_WEAPONS - 1);

	// get positions
	float Curvature = 0;
	float Speed = 0;
	const CTuningParams *pTuning = GameClient()->GetTuning(pCurrent->m_TuneZone);
	if(CurWeapon == WEAPON_GRENADE)
	{
		Curvature = pTuning->m_GrenadeCurvature;
		Speed = pTuning->m_GrenadeSpeed;
	}
	else if(CurWeapon == WEAPON_SHOTGUN)
	{
		Curvature = pTuning->m_ShotgunCurvature;
		Speed = pTuning->m_ShotgunSpeed;
	}
	else if(CurWeapon == WEAPON_GUN)
	{
		Curvature = pTuning->m_GunCurvature;
		Speed = pTuning->m_GunSpeed;
	}

	bool LocalPlayerInGame = false;

	if(m_pClient->m_Snap.m_pLocalInfo)
		LocalPlayerInGame = m_pClient->m_aClients[m_pClient->m_Snap.m_pLocalInfo->m_ClientId].m_Team != TEAM_SPECTATORS;

	static float s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);
	if(m_pClient->m_Snap.m_pGameInfoObj && !(m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED))
		s_LastGameTickTime = Client()->GameTickTime(g_Config.m_ClDummy);

	bool IsOtherTeam = (pCurrent->m_ExtraInfo && pCurrent->m_Owner >= 0 && m_pClient->IsOtherTeam(pCurrent->m_Owner));

	int PredictionTick = Client()->GetPredictionTick();

	float Ct;
	if(m_pClient->Predict() && m_pClient->AntiPingGrenade() && LocalPlayerInGame && !IsOtherTeam)
		Ct = ((float)(PredictionTick - 1 - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	else
		Ct = (Client()->PrevGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) / (float)Client()->GameTickSpeed() + s_LastGameTickTime;
	if(Ct < 0)
	{
		if(Ct > -s_LastGameTickTime / 2)
		{
			// Fixup the timing which might be screwed during demo playback because
			// s_LastGameTickTime depends on the system timer, while the other part
			// (Client()->PrevGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) / (float)Client()->GameTickSpeed()
			// is virtually constant (for projectiles fired on the current game tick):
			// (x - (x+2)) / 50 = -0.04
			//
			// We have a strict comparison for the passed time being more than the time between ticks
			// if(CurtickStart > m_Info.m_CurrentTime) in CDemoPlayer::Update()
			// so on the practice the typical value of s_LastGameTickTime varies from 0.02386 to 0.03999
			// which leads to Ct from -0.00001 to -0.01614.
			// Round up those to 0.0 to fix missing rendering of the projectile.
			Ct = 0;
		}
		else
		{
			return; // projectile haven't been shot yet
		}
	}

	vec2 Pos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, Ct);
	vec2 PrevPos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, Ct - 0.001f);

	float Alpha = 1.f;
	if(IsOtherTeam)
	{
		Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;
	}

	vec2 Vel = Pos - PrevPos;

	// add particle for this projectile
	// don't check for validity of the projectile for the current weapon here, so particle effects are rendered for mod compatibility
	if(CurWeapon == WEAPON_GRENADE)
	{
		m_pClient->m_Effects.SmokeTrail(Pos, Vel * -1, Alpha, 0.0f);
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

		Graphics()->QuadsSetRotation(s_Time * pi * 2 * 2 + ItemId);
		s_LastLocalTime = LocalTime();
	}
	else
	{
		m_pClient->m_Effects.BulletTrail(Pos, Alpha, 0.0f);

		if(length(Vel) > 0.00001f)
			Graphics()->QuadsSetRotation(angle(Vel));
		else
			Graphics()->QuadsSetRotation(0);
	}

	if(GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon].IsValid())
	{
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpriteWeaponProjectiles[CurWeapon]);
		Graphics()->SetColor(1.f, 1.f, 1.f, Alpha);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aProjectileOffset[CurWeapon], Pos.x, Pos.y);
	}
}

void CItems::RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted, int Flags)
{
	int CurWeapon = std::clamp(pCurrent->m_Subtype, 0, NUM_WEAPONS - 1);
	int QuadOffset = 2;
	float IntraTick = IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	vec2 Pos = mix(vec2(pPrev->m_X, pPrev->m_Y), vec2(pCurrent->m_X, pCurrent->m_Y), IntraTick);
	if(pCurrent->m_Type == POWERUP_HEALTH)
	{
		QuadOffset = m_PickupHealthOffset;
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupHealth);
	}
	else if(pCurrent->m_Type == POWERUP_ARMOR)
	{
		QuadOffset = m_PickupArmorOffset;
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupArmor);
	}
	else if(pCurrent->m_Type == POWERUP_WEAPON)
	{
		QuadOffset = m_aPickupWeaponOffset[CurWeapon];
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpritePickupWeapons[CurWeapon]);
	}
	else if(pCurrent->m_Type == POWERUP_NINJA)
	{
		QuadOffset = m_PickupNinjaOffset;
		if(Flags & PICKUPFLAG_ROTATE)
			m_pClient->m_Effects.PowerupShine(Pos, vec2(18, 96), 1.0f);
		else
			m_pClient->m_Effects.PowerupShine(Pos, vec2(96, 18), 1.0f);

		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpritePickupNinja);
	}
	else if(pCurrent->m_Type >= POWERUP_ARMOR_SHOTGUN && pCurrent->m_Type <= POWERUP_ARMOR_LASER)
	{
		QuadOffset = m_aPickupWeaponArmorOffset[pCurrent->m_Type - POWERUP_ARMOR_SHOTGUN];
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_aSpritePickupWeaponArmor[pCurrent->m_Type - POWERUP_ARMOR_SHOTGUN]);
	}
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	vec2 Scale = vec2(1, 1);
	if(Flags & PICKUPFLAG_XFLIP)
		Scale.x = -Scale.x;

	if(Flags & PICKUPFLAG_YFLIP)
		Scale.y = -Scale.y;

	if(Flags & PICKUPFLAG_ROTATE)
	{
		Graphics()->QuadsSetRotation(90.f * (pi / 180));
		std::swap(Scale.x, Scale.y);

		if(pCurrent->m_Type == POWERUP_NINJA)
		{
			if(Flags & PICKUPFLAG_XFLIP)
				Pos.y += 10.0f;
			else
				Pos.y -= 10.0f;
		}
	}
	else
	{
		if(pCurrent->m_Type == POWERUP_NINJA)
		{
			if(Flags & PICKUPFLAG_XFLIP)
				Pos.x += 10.0f;
			else
				Pos.x -= 10.0f;
		}
	}

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
	Pos += direction(s_Time * 2.0f + Offset) * 2.5f;
	s_LastLocalTime = LocalTime();

	Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, QuadOffset, Pos.x, Pos.y, Scale.x, Scale.y);
	Graphics()->QuadsSetRotation(0);
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
	int QuadOffset;
	if(pCurrent->m_Team == TEAM_RED)
	{
		QuadOffset = m_RedFlagOffset;
	}
	else
	{
		QuadOffset = m_BlueFlagOffset;
	}

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

void CItems::RenderLaser(const CLaserData *pCurrent, bool IsPredicted)
{
	int Type = std::clamp(pCurrent->m_Type, -1, NUM_LASERTYPES - 1);
	int ColorIn, ColorOut;
	switch(Type)
	{
	case LASERTYPE_RIFLE:
		ColorOut = g_Config.m_ClLaserRifleOutlineColor;
		ColorIn = g_Config.m_ClLaserRifleInnerColor;
		break;
	case LASERTYPE_SHOTGUN:
		ColorOut = g_Config.m_ClLaserShotgunOutlineColor;
		ColorIn = g_Config.m_ClLaserShotgunInnerColor;
		break;
	case LASERTYPE_DOOR:
		ColorOut = g_Config.m_ClLaserDoorOutlineColor;
		ColorIn = g_Config.m_ClLaserDoorInnerColor;
		break;
	case LASERTYPE_FREEZE:
		ColorOut = g_Config.m_ClLaserFreezeOutlineColor;
		ColorIn = g_Config.m_ClLaserFreezeInnerColor;
		break;
	case LASERTYPE_DRAGGER:
		ColorOut = g_Config.m_ClLaserDraggerOutlineColor;
		ColorIn = g_Config.m_ClLaserDraggerInnerColor;
		break;
	case LASERTYPE_GUN:
	case LASERTYPE_PLASMA:
		if(pCurrent->m_Subtype == LASERGUNTYPE_FREEZE || pCurrent->m_Subtype == LASERGUNTYPE_EXPFREEZE)
		{
			ColorOut = g_Config.m_ClLaserFreezeOutlineColor;
			ColorIn = g_Config.m_ClLaserFreezeInnerColor;
		}
		else
		{
			ColorOut = g_Config.m_ClLaserRifleOutlineColor;
			ColorIn = g_Config.m_ClLaserRifleInnerColor;
		}
		break;
	default:
		ColorOut = g_Config.m_ClLaserRifleOutlineColor;
		ColorIn = g_Config.m_ClLaserRifleInnerColor;
	}

	bool IsOtherTeam = (pCurrent->m_ExtraInfo && pCurrent->m_Owner >= 0 && m_pClient->IsOtherTeam(pCurrent->m_Owner));

	float Alpha = IsOtherTeam ? g_Config.m_ClShowOthersAlpha / 100.0f : 1.f;

	const ColorRGBA OuterColor = color_cast<ColorRGBA>(ColorHSLA(ColorOut).WithAlpha(Alpha));
	const ColorRGBA InnerColor = color_cast<ColorRGBA>(ColorHSLA(ColorIn).WithAlpha(Alpha));

	float Ticks;
	float TicksHead = Client()->GameTick(g_Config.m_ClDummy);
	if(Type == LASERTYPE_DOOR)
	{
		Ticks = 1.0f;
	}
	else if(IsPredicted)
	{
		int PredictionTick = Client()->GetPredictionTick();
		Ticks = (float)(PredictionTick - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy);
		TicksHead += Client()->PredIntraGameTick(g_Config.m_ClDummy);
	}
	else
	{
		Ticks = (float)(Client()->GameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) + Client()->IntraGameTick(g_Config.m_ClDummy);
		TicksHead += Client()->IntraGameTick(g_Config.m_ClDummy);
	}

	if(Type == LASERTYPE_DRAGGER)
	{
		TicksHead *= (((pCurrent->m_Subtype >> 1) % 3) * 4.0f) + 1;
		TicksHead *= (pCurrent->m_Subtype & 1) ? -1 : 1;
	}
	RenderLaser(pCurrent->m_From, pCurrent->m_To, OuterColor, InnerColor, Ticks, TicksHead, Type);
}

void CItems::RenderLaser(vec2 From, vec2 Pos, ColorRGBA OuterColor, ColorRGBA InnerColor, float TicksBody, float TicksHead, int Type) const
{
	int TuneZone = (Client()->State() == IClient::STATE_ONLINE && GameClient()->m_GameWorld.m_WorldConfig.m_UseTuneZones) ? Collision()->IsTune(Collision()->GetMapIndex(From)) : 0;
	float Len = distance(Pos, From);

	if(Len > 0)
	{
		if(Type == LASERTYPE_DRAGGER)
		{
			// rubber band effect
			float Thickness = std::sqrt(Len) / 5.f;
			TicksBody = std::clamp(Thickness, 1.0f, 5.0f);
		}
		vec2 Dir = normalize_pre_length(Pos - From, Len);

		float Ms = TicksBody * 1000.0f / Client()->GameTickSpeed();
		float a = Ms / m_pClient->GetTuning(TuneZone)->m_LaserBounceDelay;
		a = std::clamp(a, 0.0f, 1.0f);
		float Ia = 1 - a;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();

		// do outline
		Graphics()->SetColor(OuterColor);
		vec2 Out = vec2(Dir.y, -Dir.x) * (7.0f * Ia);

		IGraphics::CFreeformItem Freeform(
			From.x - Out.x, From.y - Out.y,
			From.x + Out.x, From.y + Out.y,
			Pos.x - Out.x, Pos.y - Out.y,
			Pos.x + Out.x, Pos.y + Out.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		// do inner
		Out = vec2(Dir.y, -Dir.x) * (5.0f * Ia);
		vec2 ExtraOutlinePos = Dir;
		vec2 ExtraOutlineFrom = Type == LASERTYPE_DOOR ? vec2(0, 0) : Dir;
		Graphics()->SetColor(InnerColor); // center

		Freeform = IGraphics::CFreeformItem(
			From.x - Out.x + ExtraOutlineFrom.x, From.y - Out.y + ExtraOutlineFrom.y,
			From.x + Out.x + ExtraOutlineFrom.x, From.y + Out.y + ExtraOutlineFrom.y,
			Pos.x - Out.x - ExtraOutlinePos.x, Pos.y - Out.y - ExtraOutlinePos.y,
			Pos.x + Out.x - ExtraOutlinePos.x, Pos.y + Out.y - ExtraOutlinePos.y);
		Graphics()->QuadsDrawFreeform(&Freeform, 1);

		Graphics()->QuadsEnd();
	}

	// render head
	if(Type == LASERTYPE_DOOR)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsSetRotation(0);
		Graphics()->SetColor(OuterColor);
		Graphics()->RenderQuadContainerEx(m_ItemsQuadContainerIndex, m_DoorHeadOffset, 1, Pos.x - 8.0f, Pos.y - 8.0f);
		Graphics()->SetColor(InnerColor);
		Graphics()->RenderQuadContainerEx(m_ItemsQuadContainerIndex, m_DoorHeadOffset, 1, Pos.x - 6.0f, Pos.y - 6.0f, 6.f / 8.f, 6.f / 8.f);
	}
	else if(Type == LASERTYPE_DRAGGER)
	{
		Graphics()->TextureSet(GameClient()->m_ExtrasSkin.m_SpritePulley);
		for(int Inner = 0; Inner < 2; ++Inner)
		{
			Graphics()->SetColor(Inner ? InnerColor : OuterColor);

			float Size = Inner ? 4.f / 5.f : 1.f;

			// circle at laser end
			if(Len > 0)
			{
				Graphics()->QuadsSetRotation(0);
				Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_PulleyHeadOffset, From.x, From.y, Size, Size);
			}

			//rotating orbs
			Size = Inner ? 0.75f - 1.f / 5.f : 0.75f;
			for(int Orb = 0; Orb < 3; ++Orb)
			{
				vec2 Offset(10.f, 0);
				Offset = rotate(Offset, Orb * 120 + TicksHead);
				Graphics()->QuadsSetRotation(TicksHead + Orb * pi * 2.f / 3.f); // rotate the sprite as well, as it might be customized
				Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_PulleyHeadOffset, From.x + Offset.x, From.y + Offset.y, Size, Size);
			}
		}
	}
	else if(Type == LASERTYPE_FREEZE)
	{
		float Pulsation = 6.f / 5.f + 1.f / 10.f * std::sin(TicksHead / 2.f);
		float Angle = angle(Pos - From);
		Graphics()->TextureSet(GameClient()->m_ExtrasSkin.m_SpriteHectagon);
		Graphics()->QuadsSetRotation(Angle);
		Graphics()->SetColor(OuterColor);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_FreezeHeadOffset, Pos.x, Pos.y, 6.f / 5.f * Pulsation, 6.f / 5.f * Pulsation);
		Graphics()->TextureSet(GameClient()->m_ExtrasSkin.m_SpriteParticleSnowflake);
		// snowflakes are white
		Graphics()->SetColor(ColorRGBA(1.f, 1.f, 1.f));
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_FreezeHeadOffset, Pos.x, Pos.y, Pulsation, Pulsation);
	}
	else
	{
		int CurParticle = (int)TicksHead % 3;
		Graphics()->TextureSet(GameClient()->m_ParticlesSkin.m_aSpriteParticleSplat[CurParticle]);
		Graphics()->QuadsSetRotation((int)TicksHead);
		Graphics()->SetColor(OuterColor);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aParticleSplatOffset[CurParticle], Pos.x, Pos.y);
		Graphics()->SetColor(InnerColor);
		Graphics()->RenderQuadContainerAsSprite(m_ItemsQuadContainerIndex, m_aParticleSplatOffset[CurParticle], Pos.x, Pos.y, 20.f / 24.f, 20.f / 24.f);
	}
}

void CItems::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	bool IsSuper = m_pClient->IsLocalCharSuper();
	int Ticks = Client()->GameTick(g_Config.m_ClDummy) % Client()->GameTickSpeed();
	bool BlinkingPickup = (Ticks % 22) < 4;
	bool BlinkingGun = (Ticks % 22) < 4;
	bool BlinkingDragger = (Ticks % 22) < 4;
	bool BlinkingProj = (Ticks % 20) < 2;
	bool BlinkingProjEx = (Ticks % 6) < 2;
	bool BlinkingLight = (Ticks % 6) < 2;
	int SwitcherTeam = m_pClient->SwitchStateTeam();
	int DraggerStartTick = maximum((Client()->GameTick(g_Config.m_ClDummy) / 7) * 7, Client()->GameTick(g_Config.m_ClDummy) - 4);
	int GunStartTick = (Client()->GameTick(g_Config.m_ClDummy) / 7) * 7;

	bool UsePredicted = GameClient()->Predict() && GameClient()->AntiPingGunfire();
	auto &aSwitchers = GameClient()->Switchers();
	if(UsePredicted)
	{
		for(auto *pProj = (CProjectile *)GameClient()->m_PrevPredictedWorld.FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile *)pProj->NextEntity())
		{
			if(!IsSuper && pProj->m_Number > 0 && pProj->m_Number < (int)aSwitchers.size() && !aSwitchers[pProj->m_Number].m_aStatus[SwitcherTeam] && (pProj->m_Explosive ? BlinkingProjEx : BlinkingProj))
				continue;

			CProjectileData Data = pProj->GetData();
			RenderProjectile(&Data, pProj->GetId());
		}
		for(CEntity *pEnt = GameClient()->m_PrevPredictedWorld.FindFirst(CGameWorld::ENTTYPE_LASER); pEnt; pEnt = pEnt->NextEntity())
		{
			auto *const pLaser = dynamic_cast<CLaser *>(pEnt);
			if(!pLaser || pLaser->GetOwner() < 0 || !GameClient()->m_aClients[pLaser->GetOwner()].m_IsPredictedLocal)
				continue;
			CLaserData Data = pLaser->GetData();
			RenderLaser(&Data, true);
		}
		for(auto *pPickup = (CPickup *)GameClient()->m_PrevPredictedWorld.FindFirst(CGameWorld::ENTTYPE_PICKUP); pPickup; pPickup = (CPickup *)pPickup->NextEntity())
		{
			if(!IsSuper && pPickup->m_Layer == LAYER_SWITCH && pPickup->m_Number > 0 && pPickup->m_Number < (int)aSwitchers.size() && !aSwitchers[pPickup->m_Number].m_aStatus[SwitcherTeam] && BlinkingPickup)
				continue;

			if(pPickup->InDDNetTile())
			{
				if(auto *pPrev = (CPickup *)GameClient()->m_PrevPredictedWorld.GetEntity(pPickup->GetId(), CGameWorld::ENTTYPE_PICKUP))
				{
					CNetObj_Pickup Data, Prev;
					pPickup->FillInfo(&Data);
					pPrev->FillInfo(&Prev);
					RenderPickup(&Prev, &Data, true, pPickup->Flags());
				}
			}
		}
	}

	for(const CSnapEntities &Ent : m_pClient->SnapEntities())
	{
		const IClient::CSnapItem Item = Ent.m_Item;
		const void *pData = Item.m_pData;
		const CNetObj_EntityEx *pEntEx = Ent.m_pDataEx;

		if(Item.m_Type == NETOBJTYPE_PROJECTILE || Item.m_Type == NETOBJTYPE_DDRACEPROJECTILE || Item.m_Type == NETOBJTYPE_DDNETPROJECTILE)
		{
			CProjectileData Data = ExtractProjectileInfo(Item.m_Type, pData, &GameClient()->m_GameWorld, pEntEx);
			bool Inactive = !IsSuper && Data.m_SwitchNumber > 0 && Data.m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[Data.m_SwitchNumber].m_aStatus[SwitcherTeam];
			if(Inactive && (Data.m_Explosive ? BlinkingProjEx : BlinkingProj))
				continue;
			if(UsePredicted)

			{
				if(auto *pProj = (CProjectile *)GameClient()->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData))
				{
					bool IsOtherTeam = m_pClient->IsOtherTeam(pProj->GetOwner());
					if(pProj->m_LastRenderTick <= 0 && (pProj->m_Type != WEAPON_SHOTGUN || (!pProj->m_Freeze && !pProj->m_Explosive)) // skip ddrace shotgun bullets
						&& (pProj->m_Type == WEAPON_SHOTGUN || absolute(length(pProj->m_Direction) - 1.f) < 0.02f) // workaround to skip grenades on ball mod
						&& (pProj->GetOwner() < 0 || !GameClient()->m_aClients[pProj->GetOwner()].m_IsPredictedLocal || IsOtherTeam) // skip locally predicted projectiles
						&& !Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id))
					{
						ReconstructSmokeTrail(&Data, pProj->m_DestroyTick);
					}
					pProj->m_LastRenderTick = Client()->GameTick(g_Config.m_ClDummy);
					if(!IsOtherTeam)
						continue;
				}
			}
			RenderProjectile(&Data, Item.m_Id);
		}
		else if(Item.m_Type == NETOBJTYPE_PICKUP || Item.m_Type == NETOBJTYPE_DDNETPICKUP)
		{
			CPickupData Data = ExtractPickupInfo(Item.m_Type, pData, pEntEx);
			bool Inactive = !IsSuper && Data.m_SwitchNumber > 0 && Data.m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[Data.m_SwitchNumber].m_aStatus[SwitcherTeam];

			if(Inactive && BlinkingPickup)
				continue;
			if(UsePredicted)
			{
				auto *pPickup = (CPickup *)GameClient()->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData);
				if(pPickup && pPickup->InDDNetTile())
					continue;
			}
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id);
			if(pPrev)
				RenderPickup((const CNetObj_Pickup *)pPrev, (const CNetObj_Pickup *)pData, false, Data.m_Flags);
		}
		else if(Item.m_Type == NETOBJTYPE_LASER || Item.m_Type == NETOBJTYPE_DDNETLASER)
		{
			if(UsePredicted)
			{
				auto *pLaser = dynamic_cast<CLaser *>(GameClient()->m_GameWorld.FindMatch(Item.m_Id, Item.m_Type, pData));
				if(pLaser && pLaser->GetOwner() >= 0 && GameClient()->m_aClients[pLaser->GetOwner()].m_IsPredictedLocal)
					continue;
			}

			CLaserData Data = ExtractLaserInfo(Item.m_Type, pData, &GameClient()->m_GameWorld, pEntEx);
			bool Inactive = !IsSuper && Data.m_SwitchNumber > 0 && Data.m_SwitchNumber < (int)aSwitchers.size() && !aSwitchers[Data.m_SwitchNumber].m_aStatus[SwitcherTeam];

			bool IsEntBlink = false;
			int EntStartTick = -1;
			if(Data.m_Type == LASERTYPE_FREEZE)
			{
				IsEntBlink = BlinkingLight;
				EntStartTick = DraggerStartTick;
			}
			else if(Data.m_Type == LASERTYPE_GUN)
			{
				IsEntBlink = BlinkingGun;
				EntStartTick = GunStartTick;
			}
			else if(Data.m_Type == LASERTYPE_DRAGGER)
			{
				IsEntBlink = BlinkingDragger;
				EntStartTick = DraggerStartTick;
			}
			else if(Data.m_Type == LASERTYPE_DOOR)
			{
				if(Data.m_Predict && (Inactive || IsSuper))
				{
					Data.m_From.x = Data.m_To.x;
					Data.m_From.y = Data.m_To.y;
				}
				EntStartTick = Client()->GameTick(g_Config.m_ClDummy);
			}
			else
			{
				IsEntBlink = BlinkingDragger;
				EntStartTick = Client()->GameTick(g_Config.m_ClDummy);
			}

			if(Data.m_Predict && Inactive && IsEntBlink)
			{
				continue;
			}

			if(Data.m_StartTick <= 0 && EntStartTick != -1)
			{
				Data.m_StartTick = EntStartTick;
			}

			RenderLaser(&Data);
		}
	}

	int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);

	// render flag
	for(int i = 0; i < Num; i++)
	{
		const IClient::CSnapItem Item = Client()->SnapGetItem(IClient::SNAP_CURRENT, i);

		if(Item.m_Type == NETOBJTYPE_FLAG)
		{
			const void *pPrev = Client()->SnapFindItem(IClient::SNAP_PREV, Item.m_Type, Item.m_Id);
			if(pPrev)
			{
				const void *pPrevGameData = Client()->SnapFindItem(IClient::SNAP_PREV, NETOBJTYPE_GAMEDATA, m_pClient->m_Snap.m_GameDataSnapId);
				RenderFlag(static_cast<const CNetObj_Flag *>(pPrev), static_cast<const CNetObj_Flag *>(Item.m_pData),
					static_cast<const CNetObj_GameData *>(pPrevGameData), m_pClient->m_Snap.m_pGameDataObj);
			}
		}
	}

	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
}

void CItems::OnInit()
{
	Graphics()->QuadsSetRotation(0);
	Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);

	m_ItemsQuadContainerIndex = Graphics()->CreateQuadContainer(false);

	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_RedFlagOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, -21.f, -42.f, 42.f, 84.f);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_BlueFlagOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, -21.f, -42.f, 42.f, 84.f);

	float ScaleX, ScaleY;
	RenderTools()->GetSpriteScale(SPRITE_PICKUP_HEALTH, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PickupHealthOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	RenderTools()->GetSpriteScale(SPRITE_PICKUP_ARMOR, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PickupArmorOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);

	for(int i = 0; i < NUM_WEAPONS; ++i)
	{
		RenderTools()->GetSpriteScale(g_pData->m_Weapons.m_aId[i].m_pSpriteBody, ScaleX, ScaleY);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		m_aPickupWeaponOffset[i] = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleX, g_pData->m_Weapons.m_aId[i].m_VisualSize * ScaleY);
	}
	RenderTools()->GetSpriteScale(SPRITE_PICKUP_NINJA, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PickupNinjaOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 128.f * ScaleX, 128.f * ScaleY);

	for(int i = 0; i < 4; i++)
	{
		RenderTools()->GetSpriteScale(SPRITE_PICKUP_ARMOR_SHOTGUN + i, ScaleX, ScaleY);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		m_aPickupWeaponArmorOffset[i] = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 64.f * ScaleX, 64.f * ScaleY);
	}

	for(int &ProjectileOffset : m_aProjectileOffset)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		ProjectileOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 32.f);
	}

	for(int &ParticleSplatOffset : m_aParticleSplatOffset)
	{
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		ParticleSplatOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 24.f);
	}

	RenderTools()->GetSpriteScale(SPRITE_PART_PULLEY, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_PulleyHeadOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 20.f * ScaleX);

	RenderTools()->GetSpriteScale(SPRITE_PART_HECTAGON, ScaleX, ScaleY);
	Graphics()->QuadsSetSubset(0, 0, 1, 1);
	m_FreezeHeadOffset = RenderTools()->QuadContainerAddSprite(m_ItemsQuadContainerIndex, 20.f * ScaleX);

	IGraphics::CQuadItem Brick(0, 0, 16.0f, 16.0f);
	m_DoorHeadOffset = Graphics()->QuadContainerAddQuads(m_ItemsQuadContainerIndex, &Brick, 1);

	Graphics()->QuadContainerUpload(m_ItemsQuadContainerIndex);
}

void CItems::ReconstructSmokeTrail(const CProjectileData *pCurrent, int DestroyTick)
{
	bool LocalPlayerInGame = false;

	if(m_pClient->m_Snap.m_pLocalInfo)
		LocalPlayerInGame = m_pClient->m_aClients[m_pClient->m_Snap.m_pLocalInfo->m_ClientId].m_Team != TEAM_SPECTATORS;
	if(!m_pClient->AntiPingGunfire() || !LocalPlayerInGame)
		return;

	int PredictionTick = Client()->GetPredictionTick();

	if(PredictionTick == pCurrent->m_StartTick)
		return;

	// get positions
	float Curvature = 0;
	float Speed = 0;
	const CTuningParams *pTuning = GameClient()->GetTuning(pCurrent->m_TuneZone);

	if(pCurrent->m_Type == WEAPON_GRENADE)
	{
		Curvature = pTuning->m_GrenadeCurvature;
		Speed = pTuning->m_GrenadeSpeed;
	}
	else if(pCurrent->m_Type == WEAPON_SHOTGUN)
	{
		Curvature = pTuning->m_ShotgunCurvature;
		Speed = pTuning->m_ShotgunSpeed;
	}
	else if(pCurrent->m_Type == WEAPON_GUN)
	{
		Curvature = pTuning->m_GunCurvature;
		Speed = pTuning->m_GunSpeed;
	}

	float Pt = ((float)(PredictionTick - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed();
	if(Pt < 0)
		return; // projectile haven't been shot yet

	float Gt = (Client()->PrevGameTick(g_Config.m_ClDummy) - pCurrent->m_StartTick) / (float)Client()->GameTickSpeed() + Client()->GameTickTime(g_Config.m_ClDummy);

	float Alpha = 1.f;
	if(pCurrent->m_ExtraInfo && pCurrent->m_Owner >= 0 && m_pClient->IsOtherTeam(pCurrent->m_Owner))
	{
		Alpha = g_Config.m_ClShowOthersAlpha / 100.0f;
	}

	float T = Pt;
	if(DestroyTick >= 0)
		T = minimum(Pt, ((float)(DestroyTick - 1 - pCurrent->m_StartTick) + Client()->PredIntraGameTick(g_Config.m_ClDummy)) / (float)Client()->GameTickSpeed());

	float MinTrailSpan = 0.4f * ((pCurrent->m_Type == WEAPON_GRENADE) ? 0.5f : 0.25f);
	float Step = maximum(Client()->FrameTimeAverage(), (pCurrent->m_Type == WEAPON_GRENADE) ? 0.02f : 0.01f);
	for(int i = 1 + (int)(Gt / Step); i < (int)(T / Step); i++)
	{
		float t = Step * (float)i + 0.4f * Step * random_float(-0.5f, 0.5f);
		vec2 Pos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, t);
		vec2 PrevPos = CalcPos(pCurrent->m_StartPos, pCurrent->m_StartVel, Curvature, Speed, t - 0.001f);
		vec2 Vel = Pos - PrevPos;
		float TimePassed = Pt - t;
		if(Pt - MinTrailSpan > 0.01f)
			TimePassed = minimum(TimePassed, (TimePassed - MinTrailSpan) / (Pt - MinTrailSpan) * (MinTrailSpan * 0.5f) + MinTrailSpan);
		// add particle for this projectile
		if(pCurrent->m_Type == WEAPON_GRENADE)
			m_pClient->m_Effects.SmokeTrail(Pos, Vel * -1, Alpha, TimePassed);
		else
			m_pClient->m_Effects.BulletTrail(Pos, Alpha, TimePassed);
	}
}
