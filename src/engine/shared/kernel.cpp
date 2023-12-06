/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/system.h>

#include <engine/kernel.h>

#include <vector>

class CKernel : public IKernel
{
	class CInterfaceInfo
	{
	public:
		CInterfaceInfo() :
			m_pInterface(nullptr),
			m_AutoDestroy(false)
		{
			m_aName[0] = '\0';
		}

		CInterfaceInfo(const char *pName, IInterface *pInterface, bool AutoDestroy) :
			m_pInterface(pInterface),
			m_AutoDestroy(AutoDestroy)
		{
			str_copy(m_aName, pName);
		}

		char m_aName[64];
		IInterface *m_pInterface;
		bool m_AutoDestroy;
	};

	std::vector<CInterfaceInfo> m_vInterfaces;

	CInterfaceInfo *FindInterfaceInfo(const char *pName)
	{
		for(CInterfaceInfo &Info : m_vInterfaces)
		{
			if(str_comp(pName, Info.m_aName) == 0)
				return &Info;
		}
		return nullptr;
	}

public:
	CKernel() = default;

	void Shutdown() override
	{
		for(int i = (int)m_vInterfaces.size() - 1; i >= 0; --i)
		{
			if(m_vInterfaces[i].m_AutoDestroy)
			{
				m_vInterfaces[i].m_pInterface->Shutdown();
			}
		}
	}

	~CKernel() override
	{
		// delete interfaces in reverse order just the way it would happen to objects on the stack
		for(int i = (int)m_vInterfaces.size() - 1; i >= 0; --i)
		{
			if(m_vInterfaces[i].m_AutoDestroy)
			{
				delete m_vInterfaces[i].m_pInterface;
				m_vInterfaces[i].m_pInterface = nullptr;
			}
		}
	}

	void RegisterInterfaceImpl(const char *pName, IInterface *pInterface, bool Destroy) override
	{
		dbg_assert(str_length(pName) < (int)sizeof(CInterfaceInfo().m_aName), "Interface name too long");
		dbg_assert(FindInterfaceInfo(pName) == nullptr, "Duplicate interface name");

		pInterface->m_pKernel = this;
		m_vInterfaces.emplace_back(pName, pInterface, Destroy);
	}

	void ReregisterInterfaceImpl(const char *pName, IInterface *pInterface) override
	{
		dbg_assert(FindInterfaceInfo(pName) != nullptr, "Cannot reregister interface that is not registered");
		pInterface->m_pKernel = this;
	}

	IInterface *RequestInterfaceImpl(const char *pName) override
	{
		CInterfaceInfo *pInfo = FindInterfaceInfo(pName);
		dbg_assert(pInfo != nullptr, "Interface not found");
		return pInfo->m_pInterface;
	}
};

IKernel *IKernel::Create() { return new CKernel; }
