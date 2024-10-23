#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// from Chillerbot-ux (credits to chillerbot)

// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// chillerbot-ux

MACRO_CONFIG_INT(ClFinishRename, cb_finish_rename, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change name to cb_finish_name if finish is near.")
MACRO_CONFIG_STR(ClFinishName, cb_finish_name, 16, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change to this name if cb_finish_rename is active.")
MACRO_CONFIG_INT(ClCampHack, cb_camp_hack, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "0=off 1=mark with gun 2=auto walk")
MACRO_CONFIG_STR(ClAutoReplyMsg, cb_auto_reply_msg, 32, "I'm currently tabbed out", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply when pinged in chat and cb_auto_reply is set to 1")
MACRO_CONFIG_INT(ClTabbedOutMsg, cb_tabbed_out_msg, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let others know when pinged in chat that you are tabbed out")

MACRO_CONFIG_INT(ClChillerbotHud, cb_chillerbot_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show enabled chillerbot components in hud")
MACRO_CONFIG_INT(ClChangeTileNotification, cb_change_tile_notification, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when leaving current tile type")
MACRO_CONFIG_INT(ClShowLastKiller, cb_show_last_killer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show last killer in hud requires cb_chillerbot_hud 1")
MACRO_CONFIG_INT(ClShowLastPing, cb_show_last_ping, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show last chat ping in hud requires cb_chillerbot_hud 1")
MACRO_CONFIG_INT(ClRenderLaserHead, cb_render_laser_head, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render laser bubbles")
MACRO_CONFIG_STR(ClPasswordFile, cb_password_file, 512, "chillerbot/chillpw_secret.txt", CFGFLAG_CLIENT | CFGFLAG_SAVE, "File to load passwords for autologin")
MACRO_CONFIG_INT(ClAlwaysReconnect, cb_always_reconnect, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reconnect to last server after some time no matter what")
MACRO_CONFIG_STR(ClChillerbotId, cb_chillerbot_id, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "chillerbot id do not change")
MACRO_CONFIG_INT(ClReconnectWhenEmpty, cb_reconnect_when_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reconnect to the current server when the last player leaves")
MACRO_CONFIG_INT(ClSpikeTracer, cb_spike_tracer, 0, 0, 512, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Radius in which spike tiles are traced (0=off)")
MACRO_CONFIG_INT(ClSpikeTracerWalls, cb_spike_tracer_walls, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Trace spikes through walls")
MACRO_CONFIG_INT(ClDbgIntersect, cb_dbg_intersect, 0, 0, 1, CFGFLAG_CLIENT, "Show graphical output for CCollsion::IntersectLine")
MACRO_CONFIG_INT(ClReleaseMouse, cb_release_mouse, 0, 0, 1, CFGFLAG_CLIENT, "Release the mouse (you probably do not want this)")
MACRO_CONFIG_STR(ClRunOnVoteStart, cb_run_on_vote_start, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "console command to run when a vote is called")

#if defined(CONF_CURSES_CLIENT)
MACRO_CONFIG_INT(ClTermHistory, cb_term_history, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Persist input history in filesystem for curses client")
MACRO_CONFIG_INT(ClTermBrowserSearchTop, cb_term_browser_search_top, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "When opening the search (/) in server browser show it on the top")
#endif

// skin stealer
MACRO_CONFIG_INT(ClSkinStealer, cb_skin_stealer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically adapt skin of close by tees (see also cb_skin_steal_radius)")
MACRO_CONFIG_INT(ClSkinStealRadius, cb_skin_steal_radius, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many tiles away can a tee be to get skin stolen (needs cb_skin_stealer 1)")
MACRO_CONFIG_INT(ClSkinStealColor, cb_skin_steal_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also steal skin color settings (needs cb_skin_stealer 1)")
MACRO_CONFIG_INT(ClSavedPlayerUseCustomColor, saved_player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_COL(ClSavedPlayerColorBody, saved_player_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_COL(ClSavedPlayerColorFeet, saved_player_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_STR(ClSavedPlayerSkin, saved_player_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_INT(ClSavedDummyUseCustomColor, saved_dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_COL(ClSavedDummyColorBody, saved_dummy_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_COL(ClSavedDummyColorFeet, saved_dummy_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")
MACRO_CONFIG_STR(ClSavedDummySkin, saved_dummy_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for cb_skin_stealer")

// warlist
MACRO_CONFIG_INT(ClWarList, cb_enable_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Display nameplate color based on chillerbot/warlist directory")
MACRO_CONFIG_INT(ClWarListAdvanced, cb_war_list_advanced, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Activates more complex warlist mode")
MACRO_CONFIG_INT(ClNameplatesWarReason, cb_nameplates_war_reason, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war reason in name plates")
MACRO_CONFIG_INT(ClWarListAutoReload, cb_war_list_auto_reload, 10, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reload warlist every x seconds 0=off")
MACRO_CONFIG_INT(ClSilentChatCommands, cb_silent_chat_commands, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Send a Chatmessage After Correctly Typing in a Chat Command (!help...)")





// from tater client (credits to tater)

// Render Cursor in Spectate mode
MACRO_CONFIG_INT(ClRunOnJoinConsole, tc_run_on_join_console, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to use run on join in chat or console")
MACRO_CONFIG_INT(ClRunOnJoinDelay, tc_run_on_join_delay, 2, 7, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tick Delay before using run on join")
MACRO_CONFIG_STR(ClRunOnJoinMsg, tc_run_on_join_console_msg, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What it should run inside of the Console")

MACRO_CONFIG_INT(ClLimitMouseToScreen, tc_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Limit mouse to screen boundries")



MACRO_CONFIG_INT(ClRenderCursorSpec, tc_cursor_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render your gun cursor when spectating in freeview")
MACRO_CONFIG_INT(ClDoCursorSpecOpacity, tc_do_cursor_opacity, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow Cursor Opacity Change While Spectating?")
MACRO_CONFIG_INT(ClRenderCursorSpecOpacity, tc_cursor_opacity_in_spec, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Opacity of Curser when Spectating")

// Nameplate in Spec
MACRO_CONFIG_INT(ClRenderNameplateSpec, tc_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render nameplates when spectating")

// Pingcircle next to name
MACRO_CONFIG_INT(ClPingNameCircle, tc_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle next to nameplate to indicate ping")

// Indicator Variables
MACRO_CONFIG_COL(ClIndicatorAlive, tc_indicator_alive, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of alive tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorFreeze, tc_indicator_freeze, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of frozen tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorSaved, tc_indicator_dead, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of tees who is getting saved in player indicator")
MACRO_CONFIG_INT(ClIndicatorOffset, tc_indicator_offset, 42, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Offset of indicator position")
MACRO_CONFIG_INT(ClIndicatorOffsetMax, tc_indicator_offset_max, 100, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Max indicator offset for variable offset setting")
MACRO_CONFIG_INT(ClIndicatorVariableDistance, tc_indicator_variable_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Indicator circles will be further away the further the tee is")
MACRO_CONFIG_INT(ClIndicatorMaxDistance, tc_indicator_variable_max_distance, 1000, 500, 7000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum tee distance for variable offset")
MACRO_CONFIG_INT(ClIndicatorRadius, tc_indicator_radius, 4, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) indicator circle size")
MACRO_CONFIG_INT(ClIndicatorOpacity, tc_indicator_opacity, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of indicator circles")
MACRO_CONFIG_INT(ClPlayerIndicator, tc_player_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show radial indicator of other tees")
MACRO_CONFIG_INT(ClPlayerIndicatorFreeze, tc_player_indicator_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show frozen tees in indicator")
MACRO_CONFIG_INT(ClIndicatorTeamOnly, tc_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show indicator while in team")
MACRO_CONFIG_INT(ClIndicatorTees, tc_indicator_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tees instead of circles")

// Bind Wheel
MACRO_CONFIG_INT(ClResetBindWheelMouse, tc_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening bindwheel")

// Auto Verify
MACRO_CONFIG_INT(ClAutoVerify, tc_auto_verify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto verify")

// Skin Profiles
MACRO_CONFIG_INT(ClApplyProfileSkin, tc_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply skin in profiles")
MACRO_CONFIG_INT(ClApplyProfileName, tc_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply name in profiles")
MACRO_CONFIG_INT(ClApplyProfileClan, tc_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply clan in profiles")
MACRO_CONFIG_INT(ClApplyProfileFlag, tc_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply flag in profiles")
MACRO_CONFIG_INT(ClApplyProfileColors, tc_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply colors in profiles")
MACRO_CONFIG_INT(ClApplyProfileEmote, tc_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply emote in profiles")



// Aiodob Variables


// Color Variables

	// Icon Color

	MACRO_CONFIG_COL(ClMutedColor, ac_muted_color, 12792139, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Muted Icon Color")

	// War / Team color (chillerbots Warlist)

	MACRO_CONFIG_COL(ClWarColor, ac_war_color, 16777123, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enemy Name Color")
	MACRO_CONFIG_COL(ClTeamColor, ac_team_color, 5504948, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Teammate Name Color")
	MACRO_CONFIG_COL(ClHelperColor, ac_helper_color, 2686902, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helper Name Color")

	// Misc Color
	MACRO_CONFIG_COL(ClAfkColor, ac_afk_color, 10951270, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Afk name color")
	MACRO_CONFIG_COL(ClSpecColor, ac_spec_color, 8936607, CFGFLAG_CLIENT | CFGFLAG_SAVE, "'(s)' color in scoreboard")

	// Friend / Foe Color
	MACRO_CONFIG_COL(ClFriendAfkColor, ac_friend_afk_color, 14694222, CFGFLAG_CLIENT | CFGFLAG_SAVE, "friend name color")

	MACRO_CONFIG_COL(ClFriendColor, ac_friend_color, 14745554, CFGFLAG_CLIENT | CFGFLAG_SAVE, "friend name color")
	MACRO_CONFIG_COL(ClFoeColor, ac_foe_color, 16777123, CFGFLAG_CLIENT | CFGFLAG_SAVE, "foe color (old does still work)")

	// Aiodob Menu color plates
	MACRO_CONFIG_COL(AiodobColor, Aiodob_color, 654311494, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Box Color in Aiodob Menu") // 160 70 175 228 hasalpha

// Int Variables

MACRO_CONFIG_INT(ClShowMutedInConsole, ac_show_muted_in_console, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows messaged of muted people in the console")

MACRO_CONFIG_INT(ClMutedIconNameplate, ac_muted_icon_nameplate, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows an Icon Next to Nameplates of Muted Players")
MACRO_CONFIG_INT(ClMutedIconScore, ac_muted_icon_Scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows an Icon Next to Nameplates of Muted Players ")

// chatbubble / Menu

MACRO_CONFIG_INT(ClChatBubble, ac_chatbubble, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Chatbubble on or Off")
MACRO_CONFIG_INT(ClShowOthersInMenu, ac_show_others_in_menu , 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows The Settings Emote if Someones in The Menu")
MACRO_CONFIG_INT(ClShowOwnMenuToOthers, ac_show_self_in_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show The Settings Emot to Others When In The Menu")


// misc
MACRO_CONFIG_INT(ClShowIdsChat, ac_show_ids_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to Show Client IDs in Chat")

MACRO_CONFIG_INT(ClStrongWeakColorId, ac_strong_weak_color_id, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render ClientIds in Nameplate Same Color as Strong/Weak Hook Color")
MACRO_CONFIG_INT(ClShowAiodobPreview, ac_show_preview, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Chat Preview in Aiodob Settings Menu")

MACRO_CONFIG_INT(ClDoTeammateNameColor, ac_team_name_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Teammate Nameplate Color")
MACRO_CONFIG_INT(ClDoHelperNameColor, ac_helper_name_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helper Nameplate Color")
MACRO_CONFIG_INT(ClDoEnemyNameColor, ac_war_name_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enemy Nameplate Color")
MACRO_CONFIG_INT(ClDoAfkColors, ac_do_afk_colors, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Makes Names Darker in Scoreboard if Player is afk")

MACRO_CONFIG_INT(ClScoreSpecPlayer, ac_do_score_spec_tee, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes The Tee in The Scoreboard to a Spectating Tee if The Player is Spectating")


MACRO_CONFIG_INT(ClDoWarListColorScore, ac_do_warlist_color_score, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors")


// friend name settings


MACRO_CONFIG_INT(ClDoFriendNameColor, ac_do_friend_name_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors")
MACRO_CONFIG_INT(ClDoFriendColorScore, ac_do_friend_color_score, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors in scoreboard")
MACRO_CONFIG_INT(ClDoFriendColorInchat, ac_do_friend_color_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on friend color in chat")

// unused or shouldnt be used

// foe name color (pretty much useless after I got the warlist to work)

MACRO_CONFIG_INT(ClFoeNameColor, ac_foe_name_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Foe name colors")

MACRO_CONFIG_INT(ClTest2, ac_test, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "test")
MACRO_CONFIG_INT(ClShowSpecials, ac_specials, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Autocrashes the game if someone with cheats is detected")
MACRO_CONFIG_INT(ClFinishRenameDistance, ac_finish_rename_distance, 32, 32, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, " (dont change buggy) distance to where the game detects the finish line")


MACRO_CONFIG_INT(ClScoreSpecPrefix, ac_do_score_spec_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle Spectator Prefix Next to Names in Scoreboard")
MACRO_CONFIG_INT(ClChatSpecPrefix, ac_do_chat_spec_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Spectator Prefix Next to Names in Chat")
MACRO_CONFIG_INT(ClChatEnemyPrefix, ac_do_chat_war_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Enemy Prefix  Next to Names in Chat")
MACRO_CONFIG_INT(ClChatTeammatePrefix, ac_do_chat_team_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Teammate Prefix Next to names in Chat")
MACRO_CONFIG_INT(ClChatHelperPrefix, ac_do_chat_helper_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Helper Prefix Next to names in Chat")

MACRO_CONFIG_INT(ClChatServerPrefix, ac_do_chat_server_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Server Prefix")
MACRO_CONFIG_INT(ClChatClientPrefix, ac_do_chat_client_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Client Prefix")


// string

// Prefixes

MACRO_CONFIG_STR(ClClientPrefix, ac_client_prefix, 24, "— ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client (Echo) Message Prefix")
MACRO_CONFIG_STR(ClServerPrefix, ac_server_prefix, 24, "*** ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server Message Prefix")
MACRO_CONFIG_STR(ClFriendPrefix, ac_friend_prefix, 8, "♥ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Friends in Chat (alt + num3 = ♥)")
MACRO_CONFIG_STR(ClSpecPrefix, ac_spec_prefix, 8, "(s) ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Specating People in Chat")
MACRO_CONFIG_STR(ClEnemyPrefix, ac_war_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Enemies in Chat (alt + num4 = ♦)")
MACRO_CONFIG_STR(ClTeammatePrefix, ac_team_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Teammates in Chat (alt + num4 = ♦)")
MACRO_CONFIG_STR(ClHelperPrefix, ac_helper_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Helpers in Chat (alt + num4 = ♦)")

// Custom Warlist Commands


MACRO_CONFIG_STR(ClAddHelperString, ac_addhelper_string, 12, "add_helper", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClAddMuteString, ac_addmute_string, 12, "add_mute", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClAddWarString, ac_addwar_string, 12, "add_war", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClAddTeamString, ac_addteam_string, 12, "add_team", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClRemoveHelperString, ac_delhelper_string, 12, "del_helper", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClRemoveMuteString, ac_delmute_string, 12, "del_mute", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClRemoveWarString, ac_delwar_string, 12, "del_war", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")
MACRO_CONFIG_STR(ClRemoveTeamString, ac_delteam_string, 12, "del_team", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custrom War Command String")


MACRO_CONFIG_STR(ClTest, ac_test, 24, "test", CFGFLAG_CLIENT | CFGFLAG_SAVE, "test")

// In Spec Menu Prefixes

MACRO_CONFIG_INT(ClSpecMenuFriend, ac_specmenu_friend_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Friend Name Colors in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuFriendPrefix, ac_specmenu_friend_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Friend Prefix Next to Names in The Spectate Menu")

MACRO_CONFIG_INT(ClSpecMenuHelper, ac_specmenu_helper_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Team Name Colors in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuTeammate, ac_specmenu_team_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Team Name Colors in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuEnemy, ac_specmenu_war_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on War Name Colors in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuHelperPrefix, ac_specmenu_helper_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on War Prefix Next to Names in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuEnemyPrefix, ac_specmenu_war_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on War Prefix Next to Names in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuTeammatePrefix, ac_specmenu_team prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Team Prefix Next to Names in The Spectate Menu")


// Ui

MACRO_CONFIG_INT(ClCornerRoundness, ac_corner_roundness, 100, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How round corners are in scrollable menus")

MACRO_CONFIG_INT(ClFpsSpoofer, ac_fps_spoofer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spoof Da Fps counter")

MACRO_CONFIG_INT(ClFpsSpoofPercentage, ac_fps_spoofer_percentage, 100, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fps Spoofer Percentage")


// Tee

MACRO_CONFIG_INT(ClTeeSize, cle_tee_size, 0, -5640, 4360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of tees")
MACRO_CONFIG_INT(ClTeeWalkRuntime, cle_tee_walkruntime, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "weird animations?? walk/run -time")
MACRO_CONFIG_INT(ClTeeFeetInAir, cle_tee_feet_inair, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How far the feet are apart in the air")
MACRO_CONFIG_INT(ClTeeSitting, cle_tee_sitting, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What the tee looks like while sitting (spectating, afk)")
MACRO_CONFIG_INT(ClTeeFeetWalking, cle_tee_feet_walking, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Feet while walking")

// Hook

MACRO_CONFIG_INT(ClHookSizeX, cle_hook_size_y, 0, -2, 4360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hook size x")
MACRO_CONFIG_INT(ClHookSizeY, cle_hook_size_x, 0, -2, 4360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hook size y")

// Weapons

MACRO_CONFIG_INT(ClShowWeapons, cle_show_weapons, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weapon Size")

MACRO_CONFIG_INT(ClWeaponSize, cle_weapon_size, 0, -2, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weapon Size")
MACRO_CONFIG_INT(ClWeaponRot, cle_weapon_rot, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weapon Rotation?")
MACRO_CONFIG_INT(ClWeaponRotSize2, cle_weapon_rot_size2, 0, -1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Some size? i think when looking left")
MACRO_CONFIG_INT(ClWeaponRotSize3, cle_weapon_rot_size3, 0, -1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Some size? i think when looking right")

// Hammer

MACRO_CONFIG_INT(ClHammerDir, cle_hammer_dir, 0, -4800, 5200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hammer Direction")
MACRO_CONFIG_INT(ClHammerRot, cle_hammer_rot, 0, -4800, 5200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hammer Rotation")

// Gun

MACRO_CONFIG_INT(ClGunPosSitting, cle_gun_pos_sitting, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gun Position while sitting")
MACRO_CONFIG_INT(ClGunPos, cle_gun_pos, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gun Position")
MACRO_CONFIG_INT(ClGunReload, cle_gun_reload, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gun Reload animation")
MACRO_CONFIG_INT(ClGunRecoil, cle_gun_recoil, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "how far the gun recoil animation goes")
	// Bullet
	MACRO_CONFIG_INT(ClBulletLifetime, cle_bullet_lifetime, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bullet trail Lifetime")
	MACRO_CONFIG_INT(ClBulletSize, cle_bullet_size, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bullet trail Size")

// Grenade

MACRO_CONFIG_INT(ClGrenadeLifetime, cle_grenade_lifetime, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Grenade Lifetime")
MACRO_CONFIG_INT(ClGrenadeSize, cle_grenade_size, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Grenade  Size")

// Freeze Bar

MACRO_CONFIG_INT(ClFreezeBarWidth, cle_freezebar_width, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar Width")
MACRO_CONFIG_INT(ClFreezeBarHeight, cle_freezebar_height, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar Height")

MACRO_CONFIG_INT(ClFreezeBarX, cle_freezebar_x, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar X")
MACRO_CONFIG_INT(ClFreezeBarY, cle_freezebar_y, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar Y")

// unused

MACRO_CONFIG_INT(ClSnowflakeRandomSize, cle_snowflake_random_size, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Snowflakes random size")
MACRO_CONFIG_INT(ClSnowflakeSize, cle_snowflake_size, 0, 5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Snowflake Size")

// Double Jump

MACRO_CONFIG_INT(ClDjSprite, cle_dj_sprite, 0, -8, 130, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Sprites")

MACRO_CONFIG_INT(ClDjPosX, cle_dj_pos_x, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Where Dj Spawns on The x Axis <->")
MACRO_CONFIG_INT(ClDjPosY, cle_dj_pos_y, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "where Dj Spawns on The y Axis")
MACRO_CONFIG_INT(ClDjLifespan, cle_dj_lifespan, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Lifespan")
MACRO_CONFIG_INT(ClDjSize, cle_dj_size, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Size")
MACRO_CONFIG_INT(ClDjGravity, cle_dj_gravity, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Gravity")
MACRO_CONFIG_INT(ClDjRotSpeed, cle_dj_rot_speed, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Gravity")

// snowflake

MACRO_CONFIG_INT(ClSnowflakeLifeSpan, cle_snowflake_lifespan, 0, -2, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lifespan")
MACRO_CONFIG_INT(ClSnowflakeGravity, cle_snowflake_gravity, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Snowflake Gravity")
MACRO_CONFIG_INT(ClSnowflakeCollision, cle_snowflake_collision, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let Snowflakes Collide With Floor?")

MACRO_CONFIG_INT(ClSnowflakeX, cle_snowflake_x, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Where Snowflakes Spawn on The x Axis <->")
MACRO_CONFIG_INT(ClSnowflakeY, cle_snowflake_y, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "where Snowflakes Spawn on The y Axis")


// auto reload sprites

MACRO_CONFIG_INT(ClAutoReloadSprite, cle_sprite_auto_reload, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "If Sprites Auto Reload")

// Weapon Sprites
MACRO_CONFIG_INT(ClGunSprite, cle_gun_sprite, 0, -26, 112, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Gun Uses")
MACRO_CONFIG_INT(ClHammerSprite, cle_hammer_sprite, 0, -41, 97, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Hammer Uses")
MACRO_CONFIG_INT(ClGrenadeSprite, cle_grenade_sprite, 0, -38, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Grenade Uses")
MACRO_CONFIG_INT(ClShotgunSprite, cle_shotgun_sprite, 0, -32, 106, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Shotgun Uses")
MACRO_CONFIG_INT(ClLaserSprite, cle_laser_sprite, 0, -47, 91, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Laser Uses")
MACRO_CONFIG_INT(ClNinjaSprite, cle_ninja_sprite, 0, -44, 94, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Ninja Uses")

// Cursor Sprites
MACRO_CONFIG_INT(ClGunCursorSprite, cle_gun_cursor_sprite, 0, -27, 111, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Gun Cursor Uses")
MACRO_CONFIG_INT(ClHammerCursorSprite, cle_hammer_cursor_sprite, 0, -42, 96, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Hammer Cursor Uses")
MACRO_CONFIG_INT(ClGrenadeCursorSprite, cle_grenade_cursor_sprite, 0, -39, 99, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Grenade Cursor Uses")
MACRO_CONFIG_INT(ClShotgunCursorSprite, cle_shotgun_cursor_sprite, 0, -33, 105, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Shotgun Uses")
MACRO_CONFIG_INT(ClLaserCursorSprite, cle_laser_cursor_sprite, 0, -48, 90, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Laser Cursor Uses")
MACRO_CONFIG_INT(ClNinjaCursorSprite, cle_ninja_cursor_sprite, 0, -45, 93, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Ninja Cursor Uses")

// gun fire sprite
MACRO_CONFIG_INT(ClSpriteGunFire, cle_sprite_gun_fire, 0, -29, 106, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite The Gun Fire Uses")
MACRO_CONFIG_INT(ClNoSpriteGunFire, cle_sprite_no_gun_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn off the Gun fire sprite")

// hook sprites
MACRO_CONFIG_INT(ClHookChainSprite, cle_sprite_hook_chain, 0, -50, 88, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite The Gun Fire Uses")
MACRO_CONFIG_INT(ClHookHeadSprite, cle_sprite_hook_head, 0, -51, 87, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite The Gun Fire Uses")





// Custom Vairiables from My Server for the editor
// ignore these you cant do anything with them except if you make ur own version on a server


MACRO_CONFIG_INT(SvAutoHammer, sv_auto_hammer, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoGun, sv_auto_gun, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoGrenade, sv_auto_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoLaser, sv_auto_laser, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoShotgun, sv_auto_shotgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoExplGun, sv_auto_explgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoFreezeGun, sv_auto_Freezegun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")

MACRO_CONFIG_INT(SvExplGun, sv_explgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "enables the ''better version'' of the explosion gun")

MACRO_CONFIG_INT(SvFakeGrenade, sv_fake_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Grenade doesnt explode")
MACRO_CONFIG_INT(SvDisableFreeze, sv_disable_freeze, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "fake freeze?")

MACRO_CONFIG_INT(ClDisableFreeze, cl_disable_freeze, 0, 0, 1, CFGFLAG_CLIENT, "fake freeze?")
MACRO_CONFIG_INT(ClFakeGrenade, cl_fake_grenade, 0, 0, 1, CFGFLAG_CLIENT, "Grenade doesnt explode")

