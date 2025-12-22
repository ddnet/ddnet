#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

#include <engine/shared/protocol.h>

#include <generated/protocol7.h>

class CTeeInfo
{
public:
	char m_aSkinName[MAX_SKIN_LENGTH] = "";
	bool m_UseCustomColor = false;
	int m_ColorBody = 0;
	int m_ColorFeet = 0;

	// 0.7
	char m_aaSkinPartNames[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_LENGTH] = {"", "", "", "", "", ""};
	bool m_aUseCustomColors[protocol7::NUM_SKINPARTS] = {false, false, false, false, false, false};
	int m_aSkinPartColors[protocol7::NUM_SKINPARTS] = {0, 0, 0, 0, 0, 0};

	CTeeInfo() = default;
	CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);
	CTeeInfo(const char *const apSkinPartNames[protocol7::NUM_SKINPARTS], const int aUseCustomColors[protocol7::NUM_SKINPARTS], const int aSkinPartColors[protocol7::NUM_SKINPARTS]);

	void FromSixup();
	void ToSixup();
};

#endif //GAME_SERVER_TEEINFO_H
