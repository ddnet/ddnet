#ifndef ENGINE_WARNING_H
#define ENGINE_WARNING_H

struct SWarning
{
	SWarning() :
		m_WasShown(false) {}
	SWarning(const char *pMsg) :
		m_WasShown(false)
	{
		str_copy(m_aWarningMsg, pMsg, sizeof(m_aWarningMsg));
	}
	char m_aWarningMsg[256];
	bool m_WasShown;
};

#endif
