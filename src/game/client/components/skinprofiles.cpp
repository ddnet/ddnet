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
#if defined(CONF_FAMILY_WINDOWS)
		io_write_newline(m_ProfilesFile) != 2)
#else
		io_write_newline(m_ProfilesFile) != 1)
#endif
		return;
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

	//str_copy(profile.SkinName, pSkinName, sizeof(profile.SkinName));
	//str_copy(profile.Clan, pClan, sizeof(profile.Clan));
	//str_copy(profile.Name, pName, sizeof(profile.Name));

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
	const char *pEnd = aBuf + sizeof(aBuf) - 4;
	for(int i = 0; i < m_Profiles.size(); ++i)
	{
		str_copy(aBuf, "add_profile ", sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", m_Profiles[i].BodyColor);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", m_Profiles[i].FeetColor);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", m_Profiles[i].CountryFlag);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", m_Profiles[i].Emote);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, m_Profiles[i].SkinName, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, m_Profiles[i].Name, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, m_Profiles[i].Clan, sizeof(aEscapeBuf));
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
