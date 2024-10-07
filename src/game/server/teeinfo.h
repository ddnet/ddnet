#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

#include <engine/shared/protocol.h>

class CTeeInfo
{
public:
	char m_aSkinName[MAX_SKIN_LENGTH] = "";
	int m_UseCustomColor = 0;
	int m_ColorBody = 0;
	int m_ColorFeet = 0;

	// 0.7
	char m_apSkinPartNames[6][24] = {"", "", "", "", "", ""};
	bool m_aUseCustomColors[6] = {false, false, false, false, false, false};
	int m_aSkinPartColors[6] = {0, 0, 0, 0, 0, 0};

	CTeeInfo() = default;

	CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);

	// This constructor will assume all arrays are of length 6
	CTeeInfo(const char *apSkinPartNames[6], const int *pUseCustomColors, const int *pSkinPartColors);

	void FromSixup();
	void ToSixup();
};
#endif //GAME_SERVER_TEEINFO_H
