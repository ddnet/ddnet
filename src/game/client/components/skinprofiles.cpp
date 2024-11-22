#include "skinprofiles.h"

#include <engine/storage.h>

#include <engine/config.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>

static void EscapeParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

void CSkinProfiles::WriteLine(const char *pLine)
{
	if(!m_ProfilesFile ||
		io_write(m_ProfilesFile, pLine, str_length(pLine)) != static_cast<unsigned>(str_length(pLine)) ||
		!io_write_newline(m_ProfilesFile))
	{
		return;
	}
}

void CSkinProfiles::OnInit()
{
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	IConsole *pConsole = Kernel()->RequestInterface<IConsole>();
	pConsole->Register("add_profile", "i[body] i[feet] i[flag] i[emote] s[skin] s[name] s[clan]", CFGFLAG_CLIENT, ConAddProfile, this, "Add a profile");

	IOHANDLE File = m_pStorage->OpenFile(PROFILES_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		pConsole->ExecuteFile(PROFILES_FILE);
	}
}

void CSkinProfiles::ConAddProfile(IConsole::IResult *pResult, void *pUserData)
{
	CSkinProfiles *pSelf = (CSkinProfiles *)pUserData;
	pSelf->AddProfile(pResult->GetInteger(0), pResult->GetInteger(1), pResult->GetInteger(2), pResult->GetInteger(3), pResult->GetString(4), pResult->GetString(5), pResult->GetString(6));
}

void CSkinProfiles::AddProfile(int BodyColor, int FeetColor, int CountryFlag, int Emote, const char *pSkinName, const char *pName, const char *pClan)
{
	CProfile profile = CProfile(BodyColor, FeetColor, CountryFlag, Emote, pSkinName, pName, pClan);

	// str_copy(profile.SkinName, pSkinName, sizeof(profile.SkinName));
	// str_copy(profile.Clan, pClan, sizeof(profile.Clan));
	// str_copy(profile.Name, pName, sizeof(profile.Name));

	m_Profiles.push_back(profile);
}

bool CSkinProfiles::SaveProfiles()
{
	char aBufTmp[512];
	bool Failed = false;
	m_ProfilesFile = m_pStorage->OpenFile(PROFILES_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if(!m_ProfilesFile)
	{
		dbg_msg("config", "ERROR: opening %s failed", aBufTmp);
		return false;
	}

	char aBuf[256];
	char aBufTemp[128];
	char aEscapeBuf[256];
	for(auto &Profile : m_Profiles)
	{
		str_copy(aBuf, "add_profile ", sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.BodyColor);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.FeetColor);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.CountryFlag);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.Emote);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, Profile.SkinName, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, Profile.Name, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, Profile.Clan, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\"", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		WriteLine(aBuf);
	}

	if(io_sync(m_ProfilesFile) != 0)
		Failed = true;

	if(io_close(m_ProfilesFile) != 0)
		Failed = true;

	m_ProfilesFile = {};

	if(Failed)
	{
		dbg_msg("config", "ERROR: writing to %s failed", aBufTmp);
		return false;
	}
	return true;
}
