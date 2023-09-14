//
// Created by danii on 14.09.2023.
//

#include "User.h"
#include "engine/shared/http.h"
#include <engine/shared/json.h>
#include "game/client/gameclient.h"
#include <fstream>
#include <filesystem>

bool User::login(string login, string password)
{
	string body = "{\"email\": \"" + login + "\", \"password\": \"" + password + "\"}";

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

string encrypt(string str, char key) {
	for (int i = 0; i < str.size(); i++) {
		str[i] ^= key;
	}

	return str;
}

string decrypt(string str, char key) {
	for (int i = 0; i < str.size(); i++) {
		str[i] ^= key;
	}

	return str;
}

void User::saveCredentials(string login, string password)
{
	ofstream authFile("auth.cfg");

	if (authFile.is_open()) {
		authFile << encrypt(login, 'l') << '\n' << encrypt(password, 'p') << '\n';
		authFile.close();
	}
}

void User::eraseCredentials()
{
	const std::filesystem::path file_path { "auth.cfg" };

	try {
		std::filesystem::remove(file_path);
	} catch (const std::filesystem::filesystem_error& e) {
	}
}

pair<string, string> User::getCredentials()
{
	ifstream authFile("auth.cfg");
	string login, password;

	if (authFile.is_open()) {
		getline(authFile, login);
		getline(authFile, password);
		authFile.close();
	}

	return pair<string, string>(decrypt(login, 'l'), decrypt(password, 'p'));
}

void User::logout()
{
	this->eraseCredentials();
	this->token = "";
}
