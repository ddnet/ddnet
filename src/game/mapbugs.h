#ifndef GAME_MAPBUGS_H
#define GAME_MAPBUGS_H

enum
{
	BUG_GRENADE_DOUBLEEXPLOSION=0,
	NUM_BUGS,
};

class CMapBugs
{
	friend CMapBugs GetMapBugs(const char *pName, int Size, int Crc);
	void *m_pData;

public:
	bool Contains(int Bug) const;
	void Dump() const;
};

CMapBugs GetMapBugs(const char *pName, int Size, int Crc);
#endif // GAME_MAPBUGS_H
