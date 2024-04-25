/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_KERNEL_H
#define ENGINE_KERNEL_H

class IKernel;
class IInterface;

class IInterface
{
	// friend with the kernel implementation
	friend class CKernel;
	IKernel *m_pKernel;

protected:
	IKernel *Kernel() { return m_pKernel; }

public:
	IInterface() :
		m_pKernel(nullptr) {}
	virtual void Shutdown() {}
	virtual ~IInterface() {}
};

#define MACRO_INTERFACE(Name) \
public: \
	static const char *InterfaceName() { return Name; } \
\
private:

// This kernel thingie makes the structure very flat and basically singletons.
// I'm not sure if this is a good idea but it works for now.
class IKernel
{
	// hide the implementation
	virtual void RegisterInterfaceImpl(const char *pInterfaceName, IInterface *pInterface, bool Destroy) = 0;
	virtual void ReregisterInterfaceImpl(const char *pInterfaceName, IInterface *pInterface) = 0;
	virtual IInterface *RequestInterfaceImpl(const char *pInterfaceName) = 0;

public:
	static IKernel *Create();
	virtual void Shutdown() = 0;
	virtual ~IKernel() {}

	// templated access to handle pointer conversions and interface names
	template<class TINTERFACE>
	void RegisterInterface(TINTERFACE *pInterface, bool Destroy = true)
	{
		RegisterInterfaceImpl(TINTERFACE::InterfaceName(), pInterface, Destroy);
	}
	template<class TINTERFACE>
	void ReregisterInterface(TINTERFACE *pInterface)
	{
		ReregisterInterfaceImpl(TINTERFACE::InterfaceName(), pInterface);
	}

	// Usage example:
	//		IMyInterface *pMyHandle = Kernel()->RequestInterface<IMyInterface>()
	template<class TINTERFACE>
	TINTERFACE *RequestInterface()
	{
		return reinterpret_cast<TINTERFACE *>(RequestInterfaceImpl(TINTERFACE::InterfaceName()));
	}
};

#endif
