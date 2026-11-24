#ifndef ENGINE_ASDF_H
#define ENGINE_ASDF_H

#include "kernel.h"

class CConfig;
struct NETADDR;

class IAsdf : public IInterface
{
	MACRO_INTERFACE("asdf")
public:
	virtual ~IAsdf() = default;

	virtual void Init() = 0;
	/**
	 * Checks whether the bans accessible through `IsBanned` have changed
	 * since the last time `BansHaveChanged` has been called.
	 */
	virtual bool BansHaveChanged() = 0;
	/**
	 * Checks whether a given `pAddr` is banned.
	 *
	 * @param pBuf Buffer for the ban reason, only populated if the function returns true.
	 * @param BufferSize Size of the buffer `ppBuf`.
	 * @return `true` if the given `pAddr` is banned.
	 */
	virtual bool IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const = 0;
};

IAsdf *CreateAsdf();

#endif // ENGINE_ASDF_H
