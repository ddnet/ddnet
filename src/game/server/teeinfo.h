#ifndef GAME_SERVER_TEEINFO_H
#define GAME_SERVER_TEEINFO_H

class CTeeInfo
{
public:
    char m_SkinName[64];
    int m_UseCustomColor;
    int m_ColorBody;
    int m_ColorFeet;

    // 0.7
    char m_apSkinPartNames[6][24];
    bool m_aUseCustomColors[6];
    int m_aSkinPartColors[6];

    CTeeInfo() = default;

    CTeeInfo(const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet);

    // This constructor will assume all arrays are of length 6
    CTeeInfo(const char *pSkinPartNames[6], int *pUseCustomColors, int *pSkinPartColors);

    void FromSixup();
    void ToSixup();
};
#endif //GAME_SERVER_TEEINFO_H
