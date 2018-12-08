/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PLAYERPICS_H
#define GAME_CLIENT_COMPONENTS_PLAYERPICS_H
// nobo copy and edit of countryflags.h
#include <base/vmath.h>
#include <base/tl/sorted_array.h>
#include <game/client/component.h>

class CPlayerPics : public CComponent
{
public:
	struct CPlayerPic
	{
		char m_aPlayerName[32];
		int m_Texture;

		bool operator<(const CPlayerPic &Other) { return str_comp(m_aPlayerName, Other.m_aPlayerName) < 0; }
	};

	void OnInit();

	int Num() const;
	const CPlayerPic *GetByName(const char * pName) const;
	const CPlayerPic *GetByIndex(int Index) const;
	void Render(const char * pName, const vec4 *pColor, float x, float y, float w, float h);

private:
	enum
	{
		CODE_LB=-1,
		CODE_UB=999,
		CODE_RANGE=CODE_UB-CODE_LB+1,
	};
	sorted_array<CPlayerPic> m_aPlayerPics;
	int m_CodeIndexLUT[CODE_RANGE];

	static int LoadImageByName(const char *pImgName, int IsDir, int DirType, void *pUser);
	void LoadCountryflagsIndexfile();
};
#endif
