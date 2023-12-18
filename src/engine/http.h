#ifndef HTTP_H
#define HTTP_H

#include "kernel.h"

class IHttpRequest
{
};

class IHttp : public IInterface
{
	MACRO_INTERFACE("http")

public:
	virtual void Run(std::shared_ptr<IHttpRequest> pRequest) = 0;
};

#endif
