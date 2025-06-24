#include <engine/warning.h>

#include <base/system.h>

SWarning::SWarning(const SWarning &Other)
{
	str_copy(m_aWarningTitle, Other.m_aWarningTitle);
	str_copy(m_aWarningMsg, Other.m_aWarningMsg);
	m_AutoHide = Other.m_AutoHide;
}

SWarning::SWarning(const char *pMsg)
{
	str_copy(m_aWarningMsg, pMsg);
}

SWarning::SWarning(const char *pTitle, const char *pMsg)
{
	str_copy(m_aWarningTitle, pTitle);
	str_copy(m_aWarningMsg, pMsg);
}

SWarning &SWarning::operator=(const SWarning &Other)
{
	str_copy(m_aWarningTitle, Other.m_aWarningTitle);
	str_copy(m_aWarningMsg, Other.m_aWarningMsg);
	m_AutoHide = Other.m_AutoHide;
	return *this;
}
