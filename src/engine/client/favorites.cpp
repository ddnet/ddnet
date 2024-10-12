#include <base/system.h>
#include <engine/favorites.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

class CFavorites : public IFavorites
{
protected:
	void OnConfigSave(IConfigManager *pConfigManager) override;

public:
	TRISTATE IsFavorite(const NETADDR *pAddrs, int NumAddrs) const override;
	TRISTATE IsPingAllowed(const NETADDR *pAddrs, int NumAddrs) const override;
	void Add(const NETADDR *pAddrs, int NumAddrs) override;
	void AllowPing(const NETADDR *pAddrs, int NumAddrs, bool AllowPing) override;
	void Remove(const NETADDR *pAddrs, int NumAddrs) override;
	void AllEntries(const CEntry **ppEntries, int *pNumEntries) override;

private:
	std::vector<CEntry> m_vEntries;
	std::unordered_map<NETADDR, int> m_ByAddr;

	CEntry *Entry(const NETADDR &Addr);
	const CEntry *Entry(const NETADDR &Addr) const;
	// `pEntry` must come from the `m_vEntries` vector.
	void RemoveEntry(CEntry *pEntry);
};

void CFavorites::OnConfigSave(IConfigManager *pConfigManager)
{
	for(const auto &Entry : m_vEntries)
	{
		if(Entry.m_NumAddrs > 1)
		{
			pConfigManager->WriteLine("begin_favorite_group");
		}
		for(int i = 0; i < Entry.m_NumAddrs; i++)
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			char aBuffer[128];
			net_addr_str(&Entry.m_aAddrs[i], aBuffer, sizeof(aBuffer), true);

			if(Entry.m_aAddrs[i].type & NETTYPE_TW7)
			{
				str_format(
					aAddr,
					sizeof(aAddr),
					"tw-0.7+udp://%s",
					aBuffer);
			}
			else
			{
				str_copy(aAddr, aBuffer);
			}

			if(!Entry.m_AllowPing)
			{
				str_format(aBuffer, sizeof(aBuffer), "add_favorite %s", aAddr);
			}
			else
			{
				// Add quotes to the first parameter for backward
				// compatibility with versions that took a `r` console
				// parameter.
				str_format(aBuffer, sizeof(aBuffer), "add_favorite \"%s\" allow_ping", aAddr);
			}
			pConfigManager->WriteLine(aBuffer);
		}
		if(Entry.m_NumAddrs > 1)
		{
			pConfigManager->WriteLine("end_favorite_group");
		}
	}
}

TRISTATE CFavorites::IsFavorite(const NETADDR *pAddrs, int NumAddrs) const
{
	bool All = true;
	bool None = true;
	for(int i = 0; i < NumAddrs && (All || None); i++)
	{
		const CEntry *pEntry = Entry(pAddrs[i]);
		if(pEntry)
		{
			None = false;
		}
		else
		{
			All = false;
		}
	}
	// Return ALL if no addresses were passed.
	if(All)
	{
		return TRISTATE::ALL;
	}
	else if(None)
	{
		return TRISTATE::NONE;
	}
	else
	{
		return TRISTATE::SOME;
	}
}

TRISTATE CFavorites::IsPingAllowed(const NETADDR *pAddrs, int NumAddrs) const
{
	bool All = true;
	bool None = true;
	for(int i = 0; i < NumAddrs && (All || None); i++)
	{
		const CEntry *pEntry = Entry(pAddrs[i]);
		if(pEntry == nullptr)
		{
			continue;
		}
		if(pEntry->m_AllowPing)
		{
			None = false;
		}
		else
		{
			All = false;
		}
	}
	// Return ALL if no addresses were passed.
	if(All)
	{
		return TRISTATE::ALL;
	}
	else if(None)
	{
		return TRISTATE::NONE;
	}
	else
	{
		return TRISTATE::SOME;
	}
}

void CFavorites::Add(const NETADDR *pAddrs, int NumAddrs)
{
	// First make sure that all the addresses are not registered for some
	// other favorite.
	for(int i = 0; i < NumAddrs; i++)
	{
		CEntry *pEntry = Entry(pAddrs[i]);
		if(pEntry == nullptr)
		{
			continue;
		}
		for(int j = 0; j < pEntry->m_NumAddrs; j++)
		{
			if(pEntry->m_aAddrs[j] == pAddrs[i])
			{
				pEntry->m_aAddrs[j] = pEntry->m_aAddrs[pEntry->m_NumAddrs - 1];
				pEntry->m_NumAddrs -= 1;
				break;
			}
		}
		// If the entry has become empty due to the cleaning, remove it
		// completely.
		if(pEntry->m_NumAddrs == 0)
		{
			RemoveEntry(pEntry);
		}
	}
	// Add the new entry.
	CEntry NewEntry;
	mem_zero(&NewEntry, sizeof(NewEntry));
	NewEntry.m_NumAddrs = std::min(NumAddrs, (int)std::size(NewEntry.m_aAddrs));
	for(int i = 0; i < NewEntry.m_NumAddrs; i++)
	{
		NewEntry.m_aAddrs[i] = pAddrs[i];
		m_ByAddr[pAddrs[i]] = m_vEntries.size();
	}
	NewEntry.m_AllowPing = false;
	m_vEntries.push_back(NewEntry);
}

void CFavorites::AllowPing(const NETADDR *pAddrs, int NumAddrs, bool AllowPing)
{
	for(int i = 0; i < NumAddrs; i++)
	{
		CEntry *pEntry = Entry(pAddrs[i]);
		if(pEntry == nullptr)
		{
			continue;
		}
		pEntry->m_AllowPing = AllowPing;
	}
}

void CFavorites::Remove(const NETADDR *pAddrs, int NumAddrs)
{
	for(int i = 0; i < NumAddrs; i++)
	{
		CEntry *pEntry = Entry(pAddrs[i]);
		if(pEntry == nullptr)
		{
			continue;
		}
		for(int j = 0; j < pEntry->m_NumAddrs; j++)
		{
			m_ByAddr.erase(pEntry->m_aAddrs[j]);
		}
		RemoveEntry(pEntry);
	}
}

void CFavorites::AllEntries(const CEntry **ppEntries, int *pNumEntries)
{
	*ppEntries = m_vEntries.data();
	*pNumEntries = m_vEntries.size();
}

CFavorites::CEntry *CFavorites::Entry(const NETADDR &Addr)
{
	auto Entry = m_ByAddr.find(Addr);
	if(Entry == m_ByAddr.end())
	{
		return nullptr;
	}
	return &m_vEntries[Entry->second];
}

const CFavorites::CEntry *CFavorites::Entry(const NETADDR &Addr) const
{
	auto Entry = m_ByAddr.find(Addr);
	if(Entry == m_ByAddr.end())
	{
		return nullptr;
	}
	return &m_vEntries[Entry->second];
}

void CFavorites::RemoveEntry(CEntry *pEntry)
{
	// Replace the entry
	int Index = pEntry - m_vEntries.data();
	*pEntry = m_vEntries[m_vEntries.size() - 1];
	m_vEntries.pop_back();
	if(Index != (int)m_vEntries.size())
	{
		for(int i = 0; i < pEntry->m_NumAddrs; i++)
		{
			m_ByAddr.at(pEntry->m_aAddrs[i]) = Index;
		}
	}
}

void IFavorites::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	((IFavorites *)pUserData)->OnConfigSave(pConfigManager);
}

std::unique_ptr<IFavorites> CreateFavorites()
{
	return std::make_unique<CFavorites>();
}
