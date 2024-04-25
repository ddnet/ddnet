/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_ITEMS_H
#define GAME_CLIENT_COMPONENTS_ITEMS_H
#include <game/client/component.h>
#include <game/generated/protocol.h>

class CProjectileData;
class CLaserData;

class CItems : public CComponent
{
	void RenderProjectile(const CProjectileData *pCurrent, int ItemId);
	void RenderPickup(const CNetObj_Pickup *pPrev, const CNetObj_Pickup *pCurrent, bool IsPredicted = false);
	void RenderFlag(const CNetObj_Flag *pPrev, const CNetObj_Flag *pCurrent, const CNetObj_GameData *pPrevGameData, const CNetObj_GameData *pCurGameData);
	void RenderLaser(const CLaserData *pCurrent, bool IsPredicted = false);

	int m_ItemsQuadContainerIndex;

public:
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;
	virtual void OnInit() override;

	void ReconstructSmokeTrail(const CProjectileData *pCurrent, int DestroyTick);

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
};

#endif
