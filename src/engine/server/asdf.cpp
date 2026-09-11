#include <engine/asdf.h>
#include <engine/server/asdf.h>

class CConfig;

class CAsdf : public IAsdf
{
	rust::Box<CAsdfRust> m_pInner = CAsdfRust::New();
public:
	bool HasChanged() override;
	bool IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const override;
};

IAsdf *CreateAsdf(CConfig *pConfig)
{
	return new CAsdf();
}

bool CAsdf::HasChanged()
{
	return m_pInner->HasChanged();
}

bool CAsdf::IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const
{
	rust::Str Reason;
	if(!m_pInner->IsBanned(*pAddr, Reason))
	{
		return false;
	}
	str_copy(pBuf, Reason, BufferSize);
	return true;
}
