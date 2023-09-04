//
// Created by danii on 04.09.2023.
//

#ifndef DDNET_AIMHELPER_H
#define DDNET_AIMHELPER_H

#include "game/client/component.h"

class AimHelper : public CComponent
{
public:
	int* getPlayerIdsSortedByCursor();
	int getPlayerIdForLaserShootFng();
	virtual int Sizeof() const override { return sizeof(*this); }
protected:
};

#endif // DDNET_AIMHELPER_H
