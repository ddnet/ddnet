//
// Created by danii on 14.09.2023.
//

#ifndef DDNET_USER_H
#define DDNET_USER_H

#include "engine/client.h"
#include "game/client/component.h"

#define BACKEND_URL "https://vko-abiturient.kz/authentication_token"

using namespace std;

class User : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }

	bool isAuthorized();
	bool login(string login, string password);

private:
	string token = "";
};

#endif // DDNET_USER_H
