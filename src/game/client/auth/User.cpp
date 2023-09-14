//
// Created by danii on 14.09.2023.
//

#include "User.h"
#include "engine/shared/http.h"
#include <engine/shared/json.h>
#include "game/client/gameclient.h"

bool User::login(string login, string password)
{
	string body = "{\"email\": \"" + login + "\", \"password\": \"" + password + "\"}";
//	auto request = HttpPostJson(BACKEND_URL, body.c_str());

	CHttpRequest* request = new CHttpRequest(BACKEND_URL);
	request->PostJson(body.c_str());
	request->LogProgress(HTTPLOG::FAILURE);

	Engine()->RunJobBlocking(request);

	json_value* resultJson = request->ResultJson();

	if (!resultJson) {
		return false;
	}

	const json_value &Json = *resultJson;
	const json_value &TokenString = Json["token"];

	this->token = json_string_get(&TokenString);

	return true;
}

bool User::isAuthorized()
{
	return this->token.size() > 0;
}
