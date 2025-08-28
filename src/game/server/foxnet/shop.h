#ifndef GAME_SERVER_FOXNET_COSMETICHANDLER_H
#define GAME_SERVER_FOXNET_COSMETICHANDLER_H

#include <base/system.h>
#include <vector>

class CGameContext;
class IServer;

constexpr int NUM_ITEMS = 24;
constexpr const char *Items[NUM_ITEMS] = {
	"Rainbow Feet",
	"Rainbow Body",
	"Rainbow Hook",

	"Emoticon Gun",
	"Confetti Gun",
	"Phase Gun",

	"Clockwise Indicator",
	"Counter Clockwise Indicator",
	"Inward Turning Indicator",
	"Outward Turning Indicator",
	"Line Indicator",
	"Criss Cross Indicator",

	"Explosive Death",
	"Hammer Hit Death",
	"Indicator Death",
	"Laser Death",

	"Star Trail",
	"Dot Trail",

	"Sparkle",
	"Heart Hat",
	"Inverse Aim",
	"Lovely",
	"Rotating Ball",
	"Epic Circle",
	//"Bloody"
};

constexpr const char *ItemShortcuts[NUM_ITEMS] = {
	"R_F", // Rainbow Feet
	"R_B", // Rainbow Body
	"R_H", // Rainbow Hook

	"G_E", // Emoticon Gun
	"G_C", // Confetti Gun
	"G_P", // Phase Gun

	"I_C", // Clockwise Indicator
	"I_CC", // Counter Clockwise Indicator
	"I_IT", // Inward Turning Indicator
	"I_OT", // Outward Turning Indicator
	"I_L", // Line Indicator
	"I_CrCs", // Criss Cross Indicator

	"D_E", // Explosive Death
	"D_HH", // Hammer Hit Death
	"D_I", // Indicator Death
	"D_L", // Laser Death

	"T_S", // Star Trail
	"T_D", // Dot Trail

	"O_S", // Sparkle
	"O_HH", // Heart Hat
	"O_IA", // Inverse Aim
	"O_L", // Lovely
	"O_RB", // Rotating Ball
	"O_EC", // Epic Circle
	//"Bloody"
};

enum Cosmetics
{
	C_RAINBOW_FEET = 0,
	C_RAINBOW_BODY,
	C_RAINBOW_HOOK,
	C_GUN_EMOTICON,
	C_GUN_CONFETTI,
	C_GUN_LASER,
	C_INDICATOR_CLOCKWISE,
	C_INDICATOR_COUNTERCLOCKWISE,
	C_INDICATOR_INWARD_TURNING,
	C_INDICATOR_OUTWARD_TURNING,
	C_INDICATOR_LINE,
	C_INDICATOR_CRISSCROSS,
	C_DEATH_EXPLOSIVE,
	C_DEATH_HAMMERHIT,
	C_DEATH_INDICATOR,
	C_DEATH_LASER,
	C_TRAIL_STAR,
	C_TRAIL_DOT,
	C_OTHER_SPARKLE,
	C_OTHER_HEARTHAT,
	C_OTHER_INVERSEAIM,
	C_OTHER_LOVELY,
	C_OTHER_ROTATINGBALL,
	C_OTHER_EPICCIRCLE,
	// C_OTHER_BLOODY,
	NUM_COSMETICS
};

enum ItemTypes
{
	TYPE_RAINBOW = 0,
	TYPE_GUN,
	TYPE_INDICATOR,
	TYPE_DEATHS,
	TYPE_TRAIL,
	TYPE_OTHER,
	NUM_TYPES
};

class CItems
{
	char m_Item[32] = "";
	int m_Type = 0;
	int m_Price = 0;
	int m_MinLevel = 0;

public:
	CItems(const char *pShopItem, int pItemType, int pPrice, int pMinLevel)
	{
		str_copy(m_Item, pShopItem);
		m_Type = pItemType;
		m_Price = pPrice;
		m_MinLevel = pMinLevel;
	}

	const char *Name() const { return m_Item; }
	int Type() const { return m_Type; }

	int Price() const { return m_Price; }
	int MinLevel() const { return m_MinLevel; }

	void SetPrice(int Price) { m_Price = Price; }
	void SetMinLevel(int MinLevel) { m_MinLevel = MinLevel; }

	bool operator==(const CItems &Other) const
	{
		bool NameMatch = !str_comp(Name(), Other.Name()) && str_comp(Name(), "") != 0;
		return NameMatch;
	}
};

class CShop
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;

	void AddItems();

	int GetItemPrice(const char *pName);
	int GetItemMinLevel(const char *pName);

public:

	const char *NameToShortcut(const char *pName);
	const char *ShortcutToName(const char *pShortcut);

	void BuyItem(int ClientId, const char *pName);
	std::vector<CItems *> m_Items;

	void Init(CGameContext *pGameServer);
};

#endif // GAME_SERVER_FOXNET_COSMETICHANDLER_H