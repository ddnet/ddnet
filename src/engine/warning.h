#ifndef ENGINE_WARNING_H
#define ENGINE_WARNING_H

struct SWarning
{
	SWarning() {}
	SWarning(const char *pMsg)
	{
		str_copy(m_aWarningTitle, "");
		str_copy(m_aWarningMsg, pMsg);
	}
	SWarning(const char *pTitle, const char *pMsg)
	{
		str_copy(m_aWarningTitle, pTitle);
		str_copy(m_aWarningMsg, pMsg);
	}
	char m_aWarningTitle[128];
	char m_aWarningMsg[256];
	bool m_WasShown = false;
	bool m_AutoHide = true;
};

#endif
