#include <base/dbg.h>
#include <engine/asdf.h>
#include <engine/server/asdf.h>
#include <engine/shared/config.h>


class CAsdf : public IAsdf
{
	std::optional<rust::Box<CAsdfImpl>> m_pInner = std::nullopt;
public:
	void Init() override;
	bool BansHaveChanged() override;
	bool IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const override;
};

IAsdf *CreateAsdf()
{
	return new CAsdf();
}

void CAsdf::Init()
{
	dbg_assert(!m_pInner.has_value(), "can't be initialized twice");
	m_pInner = CAsdfImpl::New(g_Config.m_SvAsdfServer);
}

bool CAsdf::BansHaveChanged()
{
	return m_pInner.value()->BansHaveChanged();
}

bool CAsdf::IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const
{
	return m_pInner.value()->IsBanned(*pAddr, rust::Slice(pBuf, BufferSize));
}
