//
// Created by Esquad on 06.09.2023.
//

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/textrender.h>

#include <engine/client/updater.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <game/generated/client_data.h>
#include <game/localization.h>

#include "menus.h"

#include <chrono>

using namespace std::chrono_literals;

void CMenus::RenderLoginMenu(CUIRect MainView)
{
	if(GameClient()->user.isAuthorized())
		return;

	CUIRect Button, LoginButton, AbortButton, PassLine, LoginLine, Label;
	CUIRect Box;
	static CButtonContainer s_Login, s_Abort;

	const float VMargin = MainView.w / 2 - 265.0f;

	MainView.VMargin(VMargin, &Box);

	Box.HSplitTop(30.0f, &Button, &Box);
	Box.Draw(ms_ColorTabbarActiveOutgame, IGraphics::CORNER_ALL, 10.0f);
	UI()->DoLabel(&Button, "You need login to use this client.", 23.0f, TEXTALIGN_CENTER);
	Box.HSplitBottom(35.0f, &Box, &Button);
	Box.HSplitBottom(35.0f, &Button, &Box);

	Box.VSplitMid(&LoginButton, &AbortButton, Box.h/2);

	LoginButton.VSplitLeft(50.0f, &Button, &LoginButton);
	AbortButton.VSplitRight(50.0f, &AbortButton, &Button);

	 std::string Login = "123";
	 std::string Pass = "qwerty";

	if(DoButton_Menu(&s_Login, "Log in", 0, &LoginButton, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.f))
	{
		GameClient()->user.login(string(m_Login), string(m_Pass));
	}

	if(DoButton_Menu(&s_Abort, "Quit", 0, &AbortButton, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.f))
		Client()->Quit();

	CUIRect LoginBox;

	MainView.VMargin(VMargin, &LoginBox);

	LoginBox.HSplitTop(LoginBox.w/3.0f, 0, &LoginBox);

	LoginBox.HSplitTop(17.0f, &Label, &LoginBox);
	UI()->DoLabel(&Label, "Login", 24.0f, TEXTALIGN_CENTER);
	LoginBox.HSplitTop(24.0f, &LoginLine, &LoginBox);
	LoginBox.HSplitTop(24.0f, &LoginLine, &LoginBox);

	LoginLine.HSplitTop(50.0f, &LoginLine, 0);
	LoginLine.HSplitBottom(50.0f, 0, &LoginLine);

	LoginLine.VSplitLeft(50.0f, 0, &LoginLine);
	LoginLine.VSplitRight(50.0f, &LoginLine, 0);


	LoginBox.HSplitTop(50.0f, &PassLine, &LoginBox);
	LoginBox.HSplitTop(17.0f, &Label, &LoginBox);
	UI()->DoLabel(&Label, "Password", 24.0f, TEXTALIGN_CENTER);
	LoginBox.HSplitTop(24.0f, &PassLine, &LoginBox);
	LoginBox.HSplitTop(7.0f, &PassLine, &LoginBox);

	PassLine.HSplitTop(50.0f, &PassLine, 0);
	PassLine.HSplitBottom(50.0f, 0, &PassLine);

	PassLine.VSplitLeft(50.0f, 0, &PassLine);
	PassLine.VSplitRight(50.0f, &PassLine, 0);



	m_LogInLogin.SetBuffer(m_Login, sizeof(m_Login));
	UI()->DoClearableEditBox(&m_LogInLogin, &LoginLine, 18.0f);

	m_LogInPassword.SetBuffer(m_Pass, sizeof(m_Pass));
	m_LogInPassword.SetHidden(true);
	UI()->DoClearableEditBox(&m_LogInPassword, &PassLine, 12.0f);

}