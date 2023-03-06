#ifndef ENGINE_SHARED_LOCALIZATION_H
#define ENGINE_SHARED_LOCALIZATION_H

/**
* An empty function that suits as a helper to identify strings that might get localized later
*/
static constexpr const char *Localizable(const char *pStr, const char *pContext = "")
{
	return pStr;
}

#endif
