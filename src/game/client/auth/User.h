//
// Created by danii on 14.09.2023.
//

#ifndef DDNET_USER_H
#define DDNET_USER_H

#include "engine/client.h"
#include "game/client/component.h"

#define BACKEND_URL "https://backend.dth.dexodus.ru/authentication_token"

using namespace std;

class User : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }

	bool isAuthorized();
	bool login(string login, string password);
	void logout();

	void saveCredentials(string login, string password);
	void eraseCredentials();
	pair<string, string> getCredentials();

private:
	string token = "";
};

#endif // DDNET_USER_H
