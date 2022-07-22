#ifndef GAME_CLIENT_COMPONENTS_SKINPROFILES_H
#define GAME_CLIENT_COMPONENTS_SKINPROFILES_H

#include <engine/console.h>
#include <engine/keys.h>
#include <engine/shared/protocol.h>
#include <vector>
#include <game/client/component.h>

#define PROFILES_FILE "tclient_profiles.cfg"

class CProfile
{
public:
	int BodyColor;
	int FeetColor;
	int CountryFlag;
	int Emote;
	char SkinName[24];
	char Name[MAX_NAME_LENGTH];
	char Clan[MAX_CLAN_LENGTH];
	CProfile(int BodyColor1, int FeetColor1, int CountryFlag1, int Emote1, const char *pSkinName, const char *pName, const char *pClan)
	{
		BodyColor = BodyColor1;
		FeetColor = FeetColor1;
		CountryFlag = CountryFlag1;
		Emote = Emote1;
		str_format(SkinName, sizeof(SkinName), "%s", pSkinName);
		str_format(Name, sizeof(Name), "%s", pName);
		str_format(Clan, sizeof(Clan), "%s", pClan);
	}
};

class CSkinProfiles : public CComponent
{
	static void ConAddProfile(IConsole::IResult* pResult, void* pUserData);
	class IStorage *m_pStorage;
	IOHANDLE m_ProfilesFile;

	public:
		std::vector<CProfile> m_Profiles;
		void AddProfile(int BodyColor, int FeetColor, int CountryFlag, int Emote, const char *pSkinName, const char *pName, const char *pClan);
		bool SaveProfiles();
		void WriteLine(const char *pLine);
		virtual int Sizeof() const override { return sizeof(*this); }
		virtual void OnInit() override;
};
#endif