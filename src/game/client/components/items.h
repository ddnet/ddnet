/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_ITEMS_H
#define GAME_CLIENT_COMPONENTS_ITEMS_H

#include <base/color.h>

#include <generated/protocol.h>

#include <game/client/component.h>

class CProjectileData;
class CLaserData;
class CTargetSwitchData;

class CItems : public CComponent
{
	void RenderProjectile(const CProjectileData *pCurrent, int ItemId);
	void RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted, int Flags);
	void RenderFlags();
	void RenderFlag(const CNetObj_Flag *pPrev, const CNetObj_Flag *pCurrent, const CNetObj_GameData *pPrevGameData, const CNetObj_GameData *pCurGameData);
	void RenderTargetSwitch(const CTargetSwitchData *pData);
	void RenderLaser(const CLaserData *pCurrent, bool IsPredicted = false);

	int m_ItemsQuadContainerIndex;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnInit() override;

	void ReconstructSmokeTrail(const CProjectileData *pCurrent, int DestroyTick);
	void RenderLaser(vec2 From, vec2 Pos, ColorRGBA OuterColor, ColorRGBA InnerColor, float TicksBody, float TicksHead, int Type) const;

private:
	int m_BlueFlagOffset;
	int m_RedFlagOffset;
	int m_PickupHealthOffset;
	int m_PickupArmorOffset;
	int m_aPickupWeaponOffset[NUM_WEAPONS];
	int m_PickupNinjaOffset;
	int m_aPickupWeaponArmorOffset[4];
	int m_aProjectileOffset[NUM_WEAPONS];
	int m_aParticleSplatOffset[3];
	int m_DoorHeadOffset;
	int m_PulleyHeadOffset;
	int m_FreezeHeadOffset;
	int m_TargetSwitchOpenOffset;
	int m_TargetSwitchCloseOffset;
};

#endif
