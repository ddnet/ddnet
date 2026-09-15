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
	virtual bool BansHaveChanged() = 0;
	virtual bool IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const = 0;
};

IAsdf *CreateAsdf();

#endif // ENGINE_ASDF_H
