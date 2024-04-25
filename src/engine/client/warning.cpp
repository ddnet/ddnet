#include <engine/warning.h>

#include <base/system.h>

SWarning::SWarning(const char *pMsg)
{
	str_copy(m_aWarningMsg, pMsg);
}
SWarning::SWarning(const char *pTitle, const char *pMsg)
{
	str_copy(m_aWarningTitle, pTitle);
	str_copy(m_aWarningMsg, pMsg);
}
