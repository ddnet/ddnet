#ifndef ENGINE_ASDF_H
#define ENGINE_ASDF_H

class CConfig;

class IAsdf : public IInterface
{
	MACRO_INTERFACE("asdf")
public:
	virtual ~IAsdf() = default;

	virtual bool HasChanged() = 0;
	virtual bool IsBanned(const NETADDR *pAddr, char *pBuf, unsigned BufferSize) const = 0;
};

IAsdf *CreateAsdf(CConfig *pConfig);

#endif // ENGINE_ASDF_H
