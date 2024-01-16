#ifndef ENGINE_WARNING_H
#define ENGINE_WARNING_H

struct SWarning
{
	SWarning() = default;
	SWarning(const char *pMsg);
	SWarning(const char *pTitle, const char *pMsg);

	char m_aWarningTitle[128] = "";
	char m_aWarningMsg[256] = "";
	bool m_WasShown = false;
	bool m_AutoHide = true;
};

#endif
