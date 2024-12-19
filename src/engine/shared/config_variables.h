/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict client movements")
MACRO_CONFIG_INT(ClPredictDummy, cl_predict_dummy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict dummy movements")
MACRO_CONFIG_INT(ClAntiPingLimit, cl_antiping_limit, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Antiping limit (0 to disable)")
MACRO_CONFIG_INT(ClAntiPing, cl_antiping, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable antiping, i. e. more aggressive prediction.")
MACRO_CONFIG_INT(ClAntiPingPlayers, cl_antiping_players, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict other player's movement more aggressively (only enabled if cl_antiping is set to 1)")
MACRO_CONFIG_INT(ClAntiPingGrenade, cl_antiping_grenade, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict grenades (only enabled if cl_antiping is set to 1)")
MACRO_CONFIG_INT(ClAntiPingWeapons, cl_antiping_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict weapon projectiles (only enabled if cl_antiping is set to 1)")
MACRO_CONFIG_INT(ClAntiPingSmooth, cl_antiping_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make the prediction of other player's movement smoother")
MACRO_CONFIG_INT(ClAntiPingGunfire, cl_antiping_gunfire, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict gunfire and show predicted weapon physics (with cl_antiping_grenade 1 and cl_antiping_weapons 1)")
MACRO_CONFIG_INT(ClPredictionMargin, cl_prediction_margin, 10, 1, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Prediction margin in ms (adds latency, can reduce lag from ping jumps)")
MACRO_CONFIG_INT(ClSubTickAiming, cl_sub_tick_aiming, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send aiming data at sub-tick accuracy")
#if defined(CONF_PLATFORM_ANDROID)
MACRO_CONFIG_INT(ClTouchControls, cl_touch_controls, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable ingame touch controls")
#else
MACRO_CONFIG_INT(ClTouchControls, cl_touch_controls, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable ingame touch controls")
#endif

MACRO_CONFIG_INT(ClNamePlates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show name plates")
MACRO_CONFIG_INT(ClNamePlatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNamePlatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use team colors for name plates")
MACRO_CONFIG_INT(ClNamePlatesSize, cl_nameplates_size, 50, -50, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the name plates")
MACRO_CONFIG_INT(ClNamePlatesClan, cl_nameplates_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan in name plates")
MACRO_CONFIG_INT(ClNamePlatesClanSize, cl_nameplates_clan_size, 30, -50, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the clan plates")
MACRO_CONFIG_INT(ClNamePlatesIds, cl_nameplates_ids, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show client IDs in name plates")
MACRO_CONFIG_INT(ClNamePlatesOwn, cl_nameplates_own, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show own name plate (useful for demo recording)")
MACRO_CONFIG_INT(ClNamePlatesFriendMark, cl_nameplates_friendmark, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show friend mark (♥) in name plates")
MACRO_CONFIG_INT(ClNamePlatesStrong, cl_nameplates_strong, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show strong/weak in name plates (0 - off, 1 - icons, 2 - icons + numbers)")
MACRO_CONFIG_INT(ClNamePlatesStrongSize, cl_nameplates_strong_size, 30, -50, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of strong/weak state icons and numbers")

MACRO_CONFIG_INT(ClAfkEmote, cl_afk_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show zzz emote next to afk players")
MACRO_CONFIG_INT(ClTextEntities, cl_text_entities, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render textual entity data")
MACRO_CONFIG_INT(ClTextEntitiesSize, cl_text_entities_size, 100, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of textual entity data from 1 to 100%")
MACRO_CONFIG_INT(ClStreamerMode, cl_streamer_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Censor sensitive information such as /save password")

MACRO_CONFIG_COL(ClAuthedPlayerColor, cl_authed_player_color, 5898211, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of name of authenticated player in scoreboard")
MACRO_CONFIG_COL(ClSameClanColor, cl_same_clan_color, 5898211, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Clan color of players with the same clan as you in scoreboard.")

MACRO_CONFIG_INT(ClEnablePingColor, cl_enable_ping_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether ping is colored in scoreboard.")
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto switch weapon on pickup")
MACRO_CONFIG_INT(ClAutoswitchWeaponsOutOfAmmo, cl_autoswitch_weapons_out_of_ammo, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto switch weapon when out of ammo")

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowhudHealthAmmo, cl_showhud_healthammo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Health + Ammo)")
MACRO_CONFIG_INT(ClShowhudScore, cl_showhud_score, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Score)")
MACRO_CONFIG_INT(ClShowhudTimer, cl_showhud_timer, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Timer)")
MACRO_CONFIG_INT(ClShowhudTimeCpDiff, cl_showhud_time_cp_diff, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Time Checkpoint Difference)")
MACRO_CONFIG_INT(ClShowhudDummyActions, cl_showhud_dummy_actions, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Actions)")
MACRO_CONFIG_INT(ClShowhudPlayerPosition, cl_showhud_player_position, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Player Position)")
MACRO_CONFIG_INT(ClShowhudPlayerSpeed, cl_showhud_player_speed, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Player Speed)")
MACRO_CONFIG_INT(ClShowhudPlayerAngle, cl_showhud_player_angle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Player Aim Angle)")
MACRO_CONFIG_INT(ClShowhudDDRace, cl_showhud_ddrace, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show ingame HUD (DDRace HUD)")
MACRO_CONFIG_INT(ClShowhudJumpsIndicator, cl_showhud_jumps_indicator, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show ingame HUD (Jumps you have and have used)")
MACRO_CONFIG_INT(ClShowFreezeBars, cl_show_freeze_bars, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to show a freeze bar under frozen players to indicate the thaw time")
MACRO_CONFIG_INT(ClFreezeBarsAlphaInsideFreeze, cl_freezebars_alpha_inside_freeze, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of freeze bars inside freeze (0 invisible, 100 fully visible)")
MACRO_CONFIG_INT(ClShowRecord, cl_showrecord, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show old style DDRace client records")
MACRO_CONFIG_INT(ClShowNotifications, cl_shownotifications, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make the client notify when someone highlights you")
MACRO_CONFIG_INT(ClShowEmotes, cl_showemotes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tee emotes")
MACRO_CONFIG_INT(ClShowChat, cl_showchat, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat (2 to always show large chat area)")
MACRO_CONFIG_INT(ClShowChatFriends, cl_show_chat_friends, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show only chat messages from friends")
MACRO_CONFIG_INT(ClShowChatTeamMembersOnly, cl_show_chat_team_members_only, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show only chat messages from team members")
MACRO_CONFIG_INT(ClShowChatSystem, cl_show_chat_system, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat messages from the server")
MACRO_CONFIG_INT(ClShowKillMessages, cl_showkillmessages, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show kill messages")
MACRO_CONFIG_INT(ClShowFinishMessages, cl_show_finish_messages, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show finish messages")
MACRO_CONFIG_INT(ClShowVotesAfterVoting, cl_show_votes_after_voting, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show votes window after voting")
MACRO_CONFIG_INT(ClShowLocalTimeAlways, cl_show_local_time_always, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Always show local time")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame FPS counter")
MACRO_CONFIG_INT(ClShowpred, cl_showpred, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame prediction time in milliseconds")
MACRO_CONFIG_INT(ClEyeWheel, cl_eye_wheel, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show eye wheel along together with emotes")
MACRO_CONFIG_INT(ClEyeDuration, cl_eye_duration, 999999, 1, 999999, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long the eyes emotes last")
MACRO_CONFIG_INT(ClFreezeStars, cl_freeze_stars, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show old star particles for frozen tees")

MACRO_CONFIG_INT(ClSpecCursor, cl_spec_cursor, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable the cursor of spectating player if available")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the air jump indicator")
MACRO_CONFIG_INT(ClThreadsoundloading, cl_threadsoundloading, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Load sound files threaded")

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 0, 0, 3000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Deadzone for the camera to follow the cursor")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Factor for the camera to follow the cursor")
MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 400, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Maximum cursor distance")
MACRO_CONFIG_INT(ClMouseMinDistance, cl_mouse_min_distance, 0, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Minimum cursor distance")

MACRO_CONFIG_INT(ClDyncam, cl_dyncam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Enable dyncam")
MACRO_CONFIG_INT(ClDyncamMaxDistance, cl_dyncam_max_distance, 1000, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Maximum dynamic camera cursor distance")
MACRO_CONFIG_INT(ClDyncamMinDistance, cl_dyncam_min_distance, 0, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Minimum dynamic camera cursor distance")
MACRO_CONFIG_INT(ClDyncamMousesens, cl_dyncam_mousesens, 0, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Mouse sens used when dyncam is toggled on")
MACRO_CONFIG_INT(ClDyncamDeadzone, cl_dyncam_deadzone, 300, 1, 1300, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Deadzone for the dynamic camera to follow the cursor")
MACRO_CONFIG_INT(ClDyncamFollowFactor, cl_dyncam_follow_factor, 60, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Factor for the dynamic camera to follow the cursor")

MACRO_CONFIG_INT(ClDyncamSmoothness, cl_dyncam_smoothness, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Transition amount of the camera movement, 0=instant, 100=slow and smooth")
MACRO_CONFIG_INT(ClDyncamStabilizing, cl_dyncam_stabilizing, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Amount of camera slowdown during fast cursor movement. High value can cause delay in camera movement")

MACRO_CONFIG_INT(ClMultiViewSensitivity, cl_multiview_sensitivity, 100, 0, 200, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Set how fast the camera will move to the desired location (higher = faster)")
MACRO_CONFIG_INT(ClMultiViewZoomSmoothness, cl_multiview_zoom_smoothness, 1300, 50, 5000, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Set the smoothness of the multi-view zoom (in ms, higher = slower)")

MACRO_CONFIG_INT(ClSpectatorMouseclicks, cl_spectator_mouseclicks, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enables left-click to toggle between spectating the closest player and free-view")
MACRO_CONFIG_INT(ClSmoothSpectatingTime, cl_smooth_spectating_time, 300, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time of smooth camera switch animation when spectating in ms (0 for off)")

MACRO_CONFIG_INT(EdAutosaveInterval, ed_autosave_interval, 10, 0, 240, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interval in minutes at which a copy of the current editor map is automatically saved to the 'auto' folder (0 for off)")
MACRO_CONFIG_INT(EdAutosaveMax, ed_autosave_max, 10, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of autosaves that are kept per map name (0 = no limit)")
MACRO_CONFIG_INT(EdSmoothZoomTime, ed_smooth_zoom_time, 250, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time of smooth zoom animation in the editor in ms (0 for off)")
MACRO_CONFIG_INT(EdLimitMaxZoomLevel, ed_limit_max_zoom_level, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies, if zooming in the editor should be limited or not (0 = no limit)")
MACRO_CONFIG_INT(EdZoomTarget, ed_zoom_target, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Zoom to the current mouse target")
MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show pressed keys")
MACRO_CONFIG_INT(EdAlignQuads, ed_align_quads, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable/disable quad alignment. When enabled, red lines appear to show how quad/points are aligned and snapped to other quads/points when moving them")
MACRO_CONFIG_INT(EdShowQuadsRect, ed_show_quads_rect, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the bounds of the selected quad. In case of multiple quads, it shows the bounds of the englobing rect. Can be helpful when aligning a group of quads")
MACRO_CONFIG_INT(EdAutoMapReload, ed_auto_map_reload, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Run 'hot_reload' on the local server while rcon authed on map save")
MACRO_CONFIG_INT(EdLayerSelector, ed_layer_selector, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ctrl+right click tiles to select their layers in the editor")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show welcome message indicating the first launch of the client")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long to show the server message of the day")

// http map download
MACRO_CONFIG_STR(ClMapDownloadUrl, cl_map_download_url, 100, "https://maps.ddnet.org", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL used to download maps (can start with http:// or https://)")
MACRO_CONFIG_INT(ClMapDownloadConnectTimeoutMs, cl_map_download_connect_timeout_ms, 2000, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP map downloads: timeout for the connect phase in milliseconds (0 to disable)")
MACRO_CONFIG_INT(ClMapDownloadLowSpeedLimit, cl_map_download_low_speed_limit, 4000, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP map downloads: Set low speed limit in bytes per second (0 to disable)")
MACRO_CONFIG_INT(ClMapDownloadLowSpeedTime, cl_map_download_low_speed_time, 3, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP map downloads: Set low speed limit time period (0 to disable)")

MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What language file to use")
MACRO_CONFIG_STR(ClSkinDownloadUrl, cl_skin_download_url, 100, "https://skins.ddnet.org/skin/", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL used to download skins")
MACRO_CONFIG_STR(ClSkinCommunityDownloadUrl, cl_skin_community_download_url, 100, "https://skins.ddnet.org/skin/community/", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL used to download community skins")
MACRO_CONFIG_INT(ClVanillaSkinsOnly, cl_vanilla_skins_only, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show skins available in Vanilla Teeworlds")
MACRO_CONFIG_INT(ClDownloadSkins, cl_download_skins, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Download skins from cl_skin_download_url on-the-fly")
MACRO_CONFIG_INT(ClDownloadCommunitySkins, cl_download_community_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow to download skins created by the community. Uses cl_skin_community_download_url instead of cl_skin_download_url for the download")
MACRO_CONFIG_INT(ClAutoStatboardScreenshot, cl_auto_statboard_screenshot, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically take game over statboard screenshot")
MACRO_CONFIG_INT(ClAutoStatboardScreenshotMax, cl_auto_statboard_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically created statboard screenshots (0 = no limit)")

MACRO_CONFIG_INT(ClDefaultZoom, cl_default_zoom, 10, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Default zoom level")
MACRO_CONFIG_INT(ClSmoothZoomTime, cl_smooth_zoom_time, 250, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time of smooth zoom animation ingame in ms (0 for off)")
MACRO_CONFIG_INT(ClLimitMaxZoomLevel, cl_limit_max_zoom_level, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies, if zooming ingame should be limited or not (0 = no limit)")

MACRO_CONFIG_INT(ClPlayerUseCustomColor, player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors")
MACRO_CONFIG_COL(ClPlayerColorBody, player_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Player body color")
MACRO_CONFIG_COL(ClPlayerColorFeet, player_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Player feet color")
MACRO_CONFIG_STR(ClPlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin")
MACRO_CONFIG_INT(ClPlayerDefaultEyes, player_default_eyes, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player eyes when joining server. 0 = normal, 1 = pain, 2 = happy, 3 = surprise, 4 = angry, 5 = blink")
MACRO_CONFIG_STR(ClSkinPrefix, cl_skin_prefix, 12, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Replace the skins by skins with this prefix (e.g. kitty, santa)")
MACRO_CONFIG_INT(ClFatSkins, cl_fat_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable fat skins")

MACRO_CONFIG_COL(ClPlayer7ColorBody, player7_color_body, 0x1B6F74, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Player body color")
MACRO_CONFIG_COL(ClPlayer7ColorFeet, player7_color_feet, 0x1C873E, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Player feet color")

MACRO_CONFIG_COL(ClPlayer7ColorMarking, player7_color_marking, 0xFF0000FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_COLALPHA | CFGFLAG_INSENSITIVE, "Player marking color")
MACRO_CONFIG_COL(ClPlayer7ColorDecoration, player7_color_decoration, 0x1B6F74, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Player decoration color")
MACRO_CONFIG_COL(ClPlayer7ColorHands, player7_color_hands, 0x1B759E, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Player hands color")
MACRO_CONFIG_COL(ClPlayer7ColorEyes, player7_color_eyes, 0x0000FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Player eyes color")
MACRO_CONFIG_INT(ClPlayer7UseCustomColorBody, player7_use_custom_color_body, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for body")
MACRO_CONFIG_INT(ClPlayer7UseCustomColorMarking, player7_use_custom_color_marking, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for marking")
MACRO_CONFIG_INT(ClPlayer7UseCustomColorDecoration, player7_use_custom_color_decoration, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for decoration")
MACRO_CONFIG_INT(ClPlayer7UseCustomColorHands, player7_use_custom_color_hands, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for hands")
MACRO_CONFIG_INT(ClPlayer7UseCustomColorFeet, player7_use_custom_color_feet, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for feet")
MACRO_CONFIG_INT(ClPlayer7UseCustomColorEyes, player7_use_custom_color_eyes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for eyes")
MACRO_CONFIG_STR(ClPlayer7Skin, player7_skin, protocol7::MAX_SKIN_ARRAY_SIZE, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin")
MACRO_CONFIG_STR(ClPlayer7SkinBody, player7_skin_body, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin body")
MACRO_CONFIG_STR(ClPlayer7SkinMarking, player7_skin_marking, protocol7::MAX_SKIN_ARRAY_SIZE, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin marking")
MACRO_CONFIG_STR(ClPlayer7SkinDecoration, player7_skin_decoration, protocol7::MAX_SKIN_ARRAY_SIZE, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin decoration")
MACRO_CONFIG_STR(ClPlayer7SkinHands, player7_skin_hands, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin hands")
MACRO_CONFIG_STR(ClPlayer7SkinFeet, player7_skin_feet, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin feet")
MACRO_CONFIG_STR(ClPlayer7SkinEyes, player7_skin_eyes, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin eyes")

MACRO_CONFIG_COL(ClDummy7ColorBody, dummy7_color_body, 0x1B6F74, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Dummy body color")
MACRO_CONFIG_COL(ClDummy7ColorFeet, dummy7_color_feet, 0x1C873E, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Dummy feet color")

MACRO_CONFIG_COL(ClDummy7ColorMarking, dummy7_color_marking, 0xFF0000FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_COLALPHA | CFGFLAG_INSENSITIVE, "Dummy marking color")
MACRO_CONFIG_COL(ClDummy7ColorDecoration, dummy7_color_decoration, 0x1B6F74, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Dummy decoration color")
MACRO_CONFIG_COL(ClDummy7ColorHands, dummy7_color_hands, 0x1B759E, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Dummy hands color")
MACRO_CONFIG_COL(ClDummy7ColorEyes, dummy7_color_eyes, 0x0000FF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT7 | CFGFLAG_INSENSITIVE, "Dummy eyes color")
MACRO_CONFIG_INT(ClDummy7UseCustomColorBody, dummy7_use_custom_color_body, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for body")
MACRO_CONFIG_INT(ClDummy7UseCustomColorMarking, dummy7_use_custom_color_marking, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for marking")
MACRO_CONFIG_INT(ClDummy7UseCustomColorDecoration, dummy7_use_custom_color_decoration, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for decoration")
MACRO_CONFIG_INT(ClDummy7UseCustomColorHands, dummy7_use_custom_color_hands, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for hands")
MACRO_CONFIG_INT(ClDummy7UseCustomColorFeet, dummy7_use_custom_color_feet, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for feet")
MACRO_CONFIG_INT(ClDummy7UseCustomColorEyes, dummy7_use_custom_color_eyes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors for eyes")
MACRO_CONFIG_STR(ClDummy7Skin, dummy7_skin, protocol7::MAX_SKIN_ARRAY_SIZE, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin")
MACRO_CONFIG_STR(ClDummy7SkinBody, dummy7_skin_body, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin body")
MACRO_CONFIG_STR(ClDummy7SkinMarking, dummy7_skin_marking, protocol7::MAX_SKIN_ARRAY_SIZE, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin marking")
MACRO_CONFIG_STR(ClDummy7SkinDecoration, dummy7_skin_decoration, protocol7::MAX_SKIN_ARRAY_SIZE, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin decoration")
MACRO_CONFIG_STR(ClDummy7SkinHands, dummy7_skin_hands, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin hands")
MACRO_CONFIG_STR(ClDummy7SkinFeet, dummy7_skin_feet, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin feet")
MACRO_CONFIG_STR(ClDummy7SkinEyes, dummy7_skin_eyes, protocol7::MAX_SKIN_ARRAY_SIZE, "standard", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin eyes")

MACRO_CONFIG_INT(UiPage, ui_page, 6, 6, 13, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface page")
MACRO_CONFIG_INT(UiSettingsPage, ui_settings_page, 0, 0, 9, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface settings page")
MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toolbox page")
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 1024, "localhost:8303", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Interface server address")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 200, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")
MACRO_CONFIG_INT(UiControllerSens, ui_controller_sens, 100, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller sensitivity for menus/editor")
MACRO_CONFIG_INT(UiSmoothScrollTime, ui_smooth_scroll_time, 500, 0, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time of smooth scrolling animation in menus/editor in ms (0 for off)")

MACRO_CONFIG_COL(UiColor, ui_color, 0xE4A046AF, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Interface color") // 160 70 175 228 hasalpha

MACRO_CONFIG_INT(UiColorizePing, ui_colorize_ping, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Highlight ping")
MACRO_CONFIG_INT(UiColorizeGametype, ui_colorize_gametype, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Highlight gametype")

MACRO_CONFIG_INT(UiCloseWindowAfterChangingSetting, ui_close_window_after_changing_setting, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Close window after changing setting")
MACRO_CONFIG_INT(UiUnreadNews, ui_unread_news, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether there is unread news")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable clipping")

// dummy
MACRO_CONFIG_STR(ClDummyName, dummy_name, 16, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Name of the dummy")
MACRO_CONFIG_STR(ClDummyClan, dummy_clan, 12, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Clan of the dummy")
MACRO_CONFIG_INT(ClDummyCountry, dummy_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Country of the Dummy")
MACRO_CONFIG_INT(ClDummyUseCustomColor, dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Toggles usage of custom colors")
MACRO_CONFIG_COL(ClDummyColorBody, dummy_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Dummy body color")
MACRO_CONFIG_COL(ClDummyColorFeet, dummy_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Dummy feet color")
MACRO_CONFIG_STR(ClDummySkin, dummy_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dummy skin")
MACRO_CONFIG_INT(ClDummyDefaultEyes, dummy_default_eyes, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dummy eyes when joining server (0 = normal, 1 = pain, 2 = happy, 3 = surprise, 4 = angry, 5 = blink)")
MACRO_CONFIG_INT(ClDummy, cl_dummy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether you control your player (0) or your dummy (1)")
MACRO_CONFIG_INT(ClDummyHammer, cl_dummy_hammer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is hammering for a hammerfly")
MACRO_CONFIG_INT(ClDummyResetOnSwitch, cl_dummy_resetonswitch, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Whether dummy or player should stop pressing keys when you switch (0 = off, 1 = dummy, 2 = player)")
MACRO_CONFIG_INT(ClDummyRestoreWeapon, cl_dummy_restore_weapon, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Whether dummy should switch to last weapon after hammerfly")
MACRO_CONFIG_INT(ClDummyCopyMoves, cl_dummy_copy_moves, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy should copy your moves")

// more controllable dummy command
MACRO_CONFIG_INT(ClDummyControl, cl_dummy_control, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether you can control dummy at the same time (cl_dummy_jump, cl_dummy_fire, cl_dummy_hook)")
MACRO_CONFIG_INT(ClDummyJump, cl_dummy_jump, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is jumping (requires cl_dummy_control 1)")
MACRO_CONFIG_INT(ClDummyFire, cl_dummy_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is firing (requires cl_dummy_control 1)")
MACRO_CONFIG_INT(ClDummyHook, cl_dummy_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Whether dummy is hooking (requires cl_dummy_control 1)")

// start menu
MACRO_CONFIG_INT(ClShowStartMenuImages, cl_show_start_menu_images, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show start menu images")
MACRO_CONFIG_INT(ClSkipStartMenu, cl_skip_start_menu, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skip the start menu")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_STR(SvGametype, sv_gametype, 32, "ddnet", CFGFLAG_SERVER, "Game type (ddnet, mod)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection for: team change, chat, skin change, emotes and votes")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 0, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 0, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time in seconds to ban a player if kicked by vote. 0 makes it just use kick")
MACRO_CONFIG_INT(SvJoinVoteDelay, sv_join_vote_delay, 300, 0, 1000, CFGFLAG_SERVER, "Add a delay before recently joined players can call any vote or participate in a kick/spec vote (in seconds)")
MACRO_CONFIG_INT(SvOldTeleportWeapons, sv_old_teleport_weapons, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Teleporting of all weapons (deprecated, use special entities instead)")
MACRO_CONFIG_INT(SvOldTeleportHook, sv_old_teleport_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Hook through teleporter (deprecated, use special entities instead)")
MACRO_CONFIG_INT(SvTeleportHoldHook, sv_teleport_hold_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Hold hook when teleported")
MACRO_CONFIG_INT(SvTeleportLoseWeapons, sv_teleport_lose_weapons, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Lose weapons when teleported (useful for some race maps)")
MACRO_CONFIG_INT(SvDeepfly, sv_deepfly, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Allow fire non auto weapons when deep")
MACRO_CONFIG_INT(SvDestroyBulletsOnDeath, sv_destroy_bullets_on_death, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Destroy bullets when their owner dies")
MACRO_CONFIG_INT(SvDestroyLasersOnDeath, sv_destroy_lasers_on_death, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Destroy lasers when their owner dies")

MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 5, 1, 100, CFGFLAG_SERVER, "64 player id <-> vanilla id players map update rate")

MACRO_CONFIG_STR(SvServerType, sv_server_type, 64, "none", CFGFLAG_SERVER, "Type of the server (novice, moderate, ...)")

MACRO_CONFIG_INT(SvSendVotesPerTick, sv_send_votes_per_tick, 5, 1, 15, CFGFLAG_SERVER, "Number of vote options being send per tick")

MACRO_CONFIG_INT(SvRescue, sv_rescue, 0, 0, 1, CFGFLAG_SERVER, "Allow /rescue command so players can teleport themselves out of freeze (setting only works in initial config)")
MACRO_CONFIG_INT(SvRescueDelay, sv_rescue_delay, 1, 0, 1000, CFGFLAG_SERVER, "Number of seconds between two rescues")
MACRO_CONFIG_INT(SvPractice, sv_practice, 1, 0, 1, CFGFLAG_SERVER, "Enable practice mode for teams. Means you can use /rescue, but in turn your rank doesn't count.")

MACRO_CONFIG_INT(ClVideoPauseWithDemo, cl_video_pausewithdemo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Pause video rendering when demo playing pause")
MACRO_CONFIG_INT(ClVideoShowhud, cl_video_showhud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD when rendering video")
MACRO_CONFIG_INT(ClVideoShowChat, cl_video_showchat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat when rendering video")
MACRO_CONFIG_INT(ClVideoSndEnable, cl_video_sound_enable, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use sound when rendering video")
MACRO_CONFIG_INT(ClVideoShowHookCollOther, cl_video_show_hook_coll_other, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show other players' hook collision lines when rendering video")
MACRO_CONFIG_INT(ClVideoShowDirection, cl_video_show_direction, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players' key presses when rendering video (1 = other players', 2 = also your own, 3 = only your own)")
MACRO_CONFIG_INT(ClVideoX264Crf, cl_video_crf, 18, 0, 51, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set crf when encode video with libx264 (0 for highest quality, 51 for lowest)")
MACRO_CONFIG_INT(ClVideoX264Preset, cl_video_preset, 5, 0, 9, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set preset when encode video with libx264, default is 5 (medium), 0 is ultrafast, 9 is placebo (the slowest, not recommend)")

// debug
#ifdef CONF_DEBUG
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Add debug dummies to server (Debug build only)")
#endif

MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 2, CFGFLAG_CLIENT, "Display information about the tuning parameters that affect the own player (0 = off, 1 = show changed, 2 = show all)")

MACRO_CONFIG_STR(PlayerName, player_name, 16, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Name of the player")
MACRO_CONFIG_STR(PlayerClan, player_clan, 12, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Clan of the player")
MACRO_CONFIG_INT(PlayerCountry, player_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Country of the player")

MACRO_CONFIG_STR(Password, password, 256, "", CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Password to the server")
MACRO_CONFIG_INT(Events, events, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Enable triggering of events, (eye emotes on some holidays in server, christmas skins in client).")
MACRO_CONFIG_STR(SteamName, steam_name, 16, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Last seen name of the Steam profile")

MACRO_CONFIG_STR(Logfile, logfile, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Filename to log all output to")
MACRO_CONFIG_INT(Logappend, logappend, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Append to logfile instead of overwriting it every time")
MACRO_CONFIG_INT(Loglevel, loglevel, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the logfile (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(StdoutOutputLevel, stdout_output_level, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the system console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(ConsoleOutputLevel, console_output_level, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the local/remote console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(ConsoleEnableColors, console_enable_colors, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Enable colors in console output")

MACRO_CONFIG_INT(ClSaveSettings, cl_save_settings, 1, 0, 1, CFGFLAG_CLIENT, "Write the settings file on exit")
MACRO_CONFIG_INT(ClRefreshRate, cl_refresh_rate, 0, 0, 10000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Refresh rate for updating the game (in Hz)")
MACRO_CONFIG_INT(ClRefreshRateInactive, cl_refresh_rate_inactive, 120, 0, 10000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Refresh rate for updating the game when the window is inactive (in Hz)")
MACRO_CONFIG_INT(ClEditor, cl_editor, 0, 0, 1, CFGFLAG_CLIENT, "Open the map editor")
MACRO_CONFIG_INT(ClEditorDilate, cl_editor_dilate, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically dilates embedded images")
MACRO_CONFIG_STR(ClSkinFilterString, cl_skin_filter_string, 25, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Skin filtering string")
MACRO_CONFIG_INT(ClEditorMaxHistory, cl_editor_max_history, 50, 1, 500, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of undo actions in the editor history (not shared between editor, envelope editor and server settings editor)")

MACRO_CONFIG_INT(ClAutoDemoRecord, cl_auto_demo_record, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically record demos")
MACRO_CONFIG_INT(ClAutoDemoOnConnect, cl_auto_demo_on_connect, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Only start a new demo when connect while automatically record demos")
MACRO_CONFIG_INT(ClAutoDemoMax, cl_auto_demo_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically recorded demos (0 = no limit)")
MACRO_CONFIG_INT(ClAutoScreenshot, cl_auto_screenshot, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically take game over screenshot")
MACRO_CONFIG_INT(ClAutoScreenshotMax, cl_auto_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically created screenshots (0 = no limit)")
MACRO_CONFIG_INT(ClAutoCSV, cl_auto_csv, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically create game over csv")
MACRO_CONFIG_INT(ClAutoCSVMax, cl_auto_csv_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically created csvs (0 = no limit)")
MACRO_CONFIG_INT(ClShowBroadcasts, cl_show_broadcasts, 1, 0, 1, CFGFLAG_CLIENT, "Show broadcasts ingame")
MACRO_CONFIG_INT(ClPrintBroadcasts, cl_print_broadcasts, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Print broadcasts to console")
MACRO_CONFIG_INT(ClPrintMotd, cl_print_motd, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Print motd to console")
MACRO_CONFIG_INT(ClFriendsIgnoreClan, cl_friends_ignore_clan, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ignore clan tag when searching for friends")

MACRO_CONFIG_STR(ClAssetsEntities, cl_assets_entities, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset/assets for entities")
MACRO_CONFIG_STR(ClAssetGame, cl_asset_game, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for game")
MACRO_CONFIG_STR(ClAssetEmoticons, cl_asset_emoticons, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for emoticons")
MACRO_CONFIG_STR(ClAssetParticles, cl_asset_particles, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for particles")
MACRO_CONFIG_STR(ClAssetHud, cl_asset_hud, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for HUD")
MACRO_CONFIG_STR(ClAssetExtras, cl_asset_extras, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for the game graphics that do not come from Teeworlds")

MACRO_CONFIG_STR(BrFilterString, br_filter_string, 128, "Novice", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Server browser filtering string")
MACRO_CONFIG_STR(BrExcludeString, br_exclude_string, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Server browser exclusion string")
MACRO_CONFIG_INT(BrFilterFull, br_filter_full, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out full server in browser")
MACRO_CONFIG_INT(BrFilterEmpty, br_filter_empty, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out empty server in browser")
MACRO_CONFIG_INT(BrFilterSpectators, br_filter_spectators, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out spectators from player numbers")
MACRO_CONFIG_INT(BrFilterFriends, br_filter_friends, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out servers with no friends")
MACRO_CONFIG_INT(BrFilterCountry, br_filter_country, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out servers with non-matching player country")
MACRO_CONFIG_INT(BrFilterCountryIndex, br_filter_country_index, -1, -1, 999, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Player country to filter by in the server browser")
MACRO_CONFIG_INT(BrFilterPw, br_filter_pw, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out password protected servers in browser")
MACRO_CONFIG_STR(BrFilterGametype, br_filter_gametype, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Game types to filter")
MACRO_CONFIG_INT(BrFilterGametypeStrict, br_filter_gametype_strict, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Strict gametype filter")
MACRO_CONFIG_INT(BrFilterConnectingPlayers, br_filter_connecting_players, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter connecting players")
MACRO_CONFIG_STR(BrFilterServerAddress, br_filter_serveraddress, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Server address to filter")
MACRO_CONFIG_INT(BrFilterUnfinishedMap, br_filter_unfinished_map, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show only servers with unfinished maps")
MACRO_CONFIG_INT(BrFilterLogin, br_filter_login, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out servers that require login")

MACRO_CONFIG_INT(BrIndicateFinished, br_indicate_finished, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show whether you have finished a DDNet map (transmits your player name to info.ddnet.org/info)")
MACRO_CONFIG_STR(BrLocation, br_location, 16, "auto", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Override location for ping estimation, available: auto, af, as, as:cn, eu, na, oc, sa (Automatic, Africa, Asia, China, Europe, North America, Oceania/Australia, South America")
MACRO_CONFIG_STR(BrCachedBestServerinfoUrl, br_cached_best_serverinfo_url, 256, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Do not set this variable, instead create a ddnet-serverlist-urls.cfg next to settings_ddnet.cfg to specify all possible serverlist URLs")

MACRO_CONFIG_INT(BrSort, br_sort, 4, 0, 256, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting column in server browser")
MACRO_CONFIG_INT(BrSortOrder, br_sort_order, 2, 0, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting order in server browser")
MACRO_CONFIG_INT(BrMaxRequests, br_max_requests, 100, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Number of concurrent requests to use when refreshing server browser")

MACRO_CONFIG_INT(BrDemoSort, br_demo_sort, 0, 0, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting column in demo browser")
MACRO_CONFIG_INT(BrDemoSortOrder, br_demo_sort_order, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting order in demo browser")
MACRO_CONFIG_INT(BrDemoFetchInfo, br_demo_fetch_info, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to auto fetch demo infos on refresh")

MACRO_CONFIG_INT(GhSort, gh_sort, 1, 0, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting column in ghost list")
MACRO_CONFIG_INT(GhSortOrder, gh_sort_order, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting order in ghost list")

MACRO_CONFIG_INT(SndBufferSize, snd_buffer_size, 512, 128, 32768, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound buffer size (may cause delay if large)")
MACRO_CONFIG_INT(SndRate, snd_rate, 48000, 5512, 384000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound mixing rate")
MACRO_CONFIG_INT(SndEnable, snd_enable, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound enable")
MACRO_CONFIG_INT(SndMusic, snd_enable_music, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Play background music")
MACRO_CONFIG_INT(SndVolume, snd_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound volume")
MACRO_CONFIG_INT(SndChatSoundVolume, snd_chat_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Chat sound volume")
MACRO_CONFIG_INT(SndGameSoundVolume, snd_game_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Game sound volume")
MACRO_CONFIG_INT(SndMapSoundVolume, snd_ambient_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Map Sound sound volume")
MACRO_CONFIG_INT(SndBackgroundMusicVolume, snd_background_music_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Background music sound volume")

MACRO_CONFIG_INT(SndNonactiveMute, snd_nonactive_mute, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mute sounds when window is not active")
MACRO_CONFIG_INT(SndGame, snd_game, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable game sounds")
MACRO_CONFIG_INT(SndGun, snd_gun, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable gun sound")
MACRO_CONFIG_INT(SndLongPain, snd_long_pain, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable long pain sound (used when shooting in freeze)")
MACRO_CONFIG_INT(SndChat, snd_chat, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable regular chat sound")
MACRO_CONFIG_INT(SndTeamChat, snd_team_chat, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable team chat sound")
MACRO_CONFIG_INT(SndServerMessage, snd_servermessage, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable server message sound")
MACRO_CONFIG_INT(SndHighlight, snd_highlight, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable highlighted chat sound")

MACRO_CONFIG_INT(GfxScreen, gfx_screen, 0, 0, 15, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen index")
MACRO_CONFIG_INT(GfxScreenWidth, gfx_screen_width, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen resolution width")
MACRO_CONFIG_INT(GfxScreenHeight, gfx_screen_height, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen resolution height")
MACRO_CONFIG_INT(GfxScreenRefreshRate, gfx_screen_refresh_rate, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen refresh rate")

MACRO_CONFIG_INT(GfxDesktopWidth, gfx_desktop_width, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Desktop resolution width for detecting display changes (not recommended to change manually)")
MACRO_CONFIG_INT(GfxDesktopHeight, gfx_desktop_height, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Desktop resolution height for detecting display changes (not recommended to change manually)")
#if !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_INT(GfxBorderless, gfx_borderless, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Borderless window (not to be used with fullscreen)")
#if !defined(CONF_WEBASM)
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 1, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Set fullscreen mode: 0=no fullscreen, 1=pure fullscreen, 2=desktop fullscreen, 3=windowed fullscreen")
#else
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 0, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Set fullscreen mode: 0=no fullscreen, 1=pure fullscreen, 2=desktop fullscreen, 3=windowed fullscreen")
#endif
#else
MACRO_CONFIG_INT(GfxBorderless, gfx_borderless, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Borderless window (not to be used with fullscreen)")
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 0, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Set fullscreen mode: 0=no fullscreen, 1=pure fullscreen, 2=desktop fullscreen, 3=windowed fullscreen")
#endif
MACRO_CONFIG_INT(GfxHighdpi, gfx_highdpi, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable high-dpi")
MACRO_CONFIG_INT(GfxColorDepth, gfx_color_depth, 24, 16, 24, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Colors bits for framebuffer (fullscreen only)")
MACRO_CONFIG_INT(GfxVsync, gfx_vsync, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Vertical sync (may cause delay)")
MACRO_CONFIG_INT(GfxDisplayAllVideoModes, gfx_display_all_video_modes, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show all video modes")
MACRO_CONFIG_INT(GfxHighDetail, gfx_high_detail, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "High detail")
MACRO_CONFIG_INT(GfxFsaaSamples, gfx_fsaa_samples, 0, 0, 64, CFGFLAG_SAVE | CFGFLAG_CLIENT, "FSAA samples (may cause delay)")
MACRO_CONFIG_INT(GfxRefreshRate, gfx_refresh_rate, 0, 0, 10000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen refresh rate")
MACRO_CONFIG_INT(GfxBackgroundRender, gfx_backgroundrender, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Render graphics when window is in background")
MACRO_CONFIG_INT(GfxTextOverlay, gfx_text_overlay, 10, 1, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Stop rendering textoverlay in editor or with entities: high value = less details = more speed")
MACRO_CONFIG_INT(GfxAsyncRenderOld, gfx_asyncrender_old, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "During an update cycle, skip the render cycle, if the render cycle would need to wait for the previous render cycle to finish")
MACRO_CONFIG_INT(GfxQuadAsTriangle, gfx_quad_as_triangle, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Render quads as triangles (fixes quad coloring on some GPUs)")

MACRO_CONFIG_INT(InpMousesens, inp_mousesens, 200, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mouse sensitivity")
MACRO_CONFIG_INT(InpTranslatedKeys, inp_translated_keys, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Translate keys before interpreting them, respects keyboard layouts")
MACRO_CONFIG_INT(InpIgnoredModifiers, inp_ignored_modifiers, 0, 0, 65536, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Ignored keyboard modifier mask")
#if defined(CONF_FAMILY_WINDOWS)
MACRO_CONFIG_INT(InpImeNativeUi, inp_ime_native_ui, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Use native UI for IME (may cause IME to not work in fullscreen mode) (changing requires restart)")
#endif

MACRO_CONFIG_INT(InpControllerEnable, inp_controller_enable, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable controller")
MACRO_CONFIG_STR(InpControllerGUID, inp_controller_guid, 34, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller GUID which uniquely identifies the active controller")
MACRO_CONFIG_INT(InpControllerAbsolute, inp_controller_absolute, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable absolute controller aiming ingame")
MACRO_CONFIG_INT(InpControllerSens, inp_controller_sens, 100, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Ingame controller sensitivity")
MACRO_CONFIG_INT(InpControllerX, inp_controller_x, 0, 0, 12, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller axis that controls X axis of cursor")
MACRO_CONFIG_INT(InpControllerY, inp_controller_y, 1, 0, 12, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller axis that controls Y axis of cursor")
MACRO_CONFIG_INT(InpControllerTolerance, inp_controller_tolerance, 5, 0, 50, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller axis tolerance to account for jitter")

MACRO_CONFIG_INT(ClPort, cl_port, 0, 0, 65535, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Port to use for client connections to server (0 to choose a random port, 1024 or higher to set a manual port, requires a restart)")
MACRO_CONFIG_INT(ClDummyPort, cl_dummy_port, 0, 0, 65535, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Port to use for dummy connections to server (0 to choose a random port, 1024 or higher to set a manual port, requires a restart)")
MACRO_CONFIG_INT(ClContactPort, cl_contact_port, 0, 0, 65535, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Port to use for serverinfo connections to server (0 to choose a random port, 1024 or higher to set a manual port, requires a restart)")

MACRO_CONFIG_STR(SvName, sv_name, 128, "unnamed server", CFGFLAG_SERVER, "Server name")
MACRO_CONFIG_STR(Bindaddr, bindaddr, 128, "", CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_MASTER, "Address to bind the client/server to")
MACRO_CONFIG_INT(SvIpv4Only, sv_ipv4only, 0, 0, 1, CFGFLAG_SERVER, "Whether to bind only to ipv4, otherwise bind to all available interfaces")
MACRO_CONFIG_INT(SvPort, sv_port, 0, 0, 65535, CFGFLAG_SERVER, "Port to use for the server (Only ports 8303-8310 work in LAN server browser, 0 to automatically find a free port in 8303-8310)")
MACRO_CONFIG_STR(SvHostname, sv_hostname, 128, "", CFGFLAG_SERVER, "Server hostname (0.7 only)")
MACRO_CONFIG_STR(SvMap, sv_map, 128, "Sunny Side Up", CFGFLAG_SERVER, "Map to use on the server")
MACRO_CONFIG_INT(SvMaxClients, sv_max_clients, MAX_CLIENTS, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients that are allowed on a server")
MACRO_CONFIG_INT(SvMaxClientsPerIp, sv_max_clients_per_ip, 4, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients with the same IP that can connect to the server")
MACRO_CONFIG_INT(SvHighBandwidth, sv_high_bandwidth, 0, 0, 1, CFGFLAG_SERVER, "Use high bandwidth mode. Doubles the bandwidth required for the server. LAN use only")
MACRO_CONFIG_STR(SvRegister, sv_register, 16, "1", CFGFLAG_SERVER, "Register server with master server for public listing, can also accept a comma-separated list of protocols to register on, like 'ipv4,ipv6'")
MACRO_CONFIG_STR(SvRegisterExtra, sv_register_extra, 256, "", CFGFLAG_SERVER, "Extra headers to send to the register endpoint, comma separated 'Header: Value' pairs")
MACRO_CONFIG_STR(SvRegisterUrl, sv_register_url, 128, "https://master1.ddnet.org/ddnet/15/register", CFGFLAG_SERVER, "Masterserver URL to register to")
MACRO_CONFIG_STR(SvMapsBaseUrl, sv_maps_base_url, 128, "", CFGFLAG_SERVER, "Base path used to provide HTTPS map download URL to the clients")
MACRO_CONFIG_STR(SvRconPassword, sv_rcon_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password (full access)")
MACRO_CONFIG_STR(SvRconModPassword, sv_rcon_mod_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password for moderators (limited access)")
MACRO_CONFIG_STR(SvRconHelperPassword, sv_rcon_helper_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password for helpers (limited access)")
MACRO_CONFIG_INT(SvRconMaxTries, sv_rcon_max_tries, 30, 0, 100, CFGFLAG_SERVER, "Maximum number of tries for remote console authentication")
MACRO_CONFIG_INT(SvRconBantime, sv_rcon_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time a client gets banned if remote console authentication fails. 0 makes it just use kick")
MACRO_CONFIG_INT(SvAutoDemoRecord, sv_auto_demo_record, 0, 0, 1, CFGFLAG_SERVER, "Automatically record demos")
MACRO_CONFIG_INT(SvAutoDemoMax, sv_auto_demo_max, 10, 0, 1000, CFGFLAG_SERVER, "Maximum number of automatically recorded demos (0 = no limit)")
MACRO_CONFIG_INT(SvTeeHistorian, sv_tee_historian, 0, 0, 1, CFGFLAG_SERVER, "Activate the tee historian that writes complete gameplay data to disk (WARNING: This will use a lot of disk space)")
MACRO_CONFIG_INT(SvVanillaAntiSpoof, sv_vanilla_antispoof, 1, 0, 1, CFGFLAG_SERVER, "Enable vanilla Antispoof")
MACRO_CONFIG_INT(SvDnsbl, sv_dnsbl, 0, 0, 1, CFGFLAG_SERVER, "Enable DNSBL (DNS-based Blackhole List)")
MACRO_CONFIG_STR(SvDnsblHost, sv_dnsbl_host, 128, "", CFGFLAG_SERVER, "Hostname of DNSBL provider to use for IP Verification")
MACRO_CONFIG_STR(SvDnsblKey, sv_dnsbl_key, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Optional Authentication Key for the specified DNSBL provider")
MACRO_CONFIG_INT(SvDnsblVote, sv_dnsbl_vote, 0, 0, 1, CFGFLAG_SERVER, "Block votes by blacklisted addresses")
MACRO_CONFIG_INT(SvDnsblBan, sv_dnsbl_ban, 0, 0, 1, CFGFLAG_SERVER, "Automatically ban blacklisted addresses")
MACRO_CONFIG_STR(SvDnsblBanReason, sv_dnsbl_ban_reason, 128, "VPN detected, try connecting without. Contact admin if mistaken", CFGFLAG_SERVER, "Ban reason for 'sv_dnsbl_ban'")
MACRO_CONFIG_INT(SvDnsblChat, sv_dnsbl_chat, 0, 0, 1, CFGFLAG_SERVER, "Don't allow chat from blacklisted addresses")
MACRO_CONFIG_INT(SvRconVote, sv_rcon_vote, 0, 0, 1, CFGFLAG_SERVER, "Only allow authed clients to call votes")

MACRO_CONFIG_INT(SvPlayerDemoRecord, sv_player_demo_record, 0, 0, 1, CFGFLAG_SERVER, "Automatically record demos for each player")
MACRO_CONFIG_INT(SvDemoChat, sv_demo_chat, 0, 0, 1, CFGFLAG_SERVER, "Record chat for demos")
MACRO_CONFIG_INT(SvServerInfoPerSecond, sv_server_info_per_second, 50, 0, 10000, CFGFLAG_SERVER, "Maximum number of complete server info responses that are sent out per second (0 for no limit)")
MACRO_CONFIG_INT(SvVanConnPerSecond, sv_van_conn_per_second, 10, 0, 10000, CFGFLAG_SERVER, "Antispoof specific ratelimit (0 for no limit)")
MACRO_CONFIG_INT(SvSixup, sv_sixup, 1, 0, 1, CFGFLAG_SERVER, "Enable sixup connections")
MACRO_CONFIG_INT(SvSkillLevel, sv_skill_level, 1, SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX, CFGFLAG_SERVER, "Difficulty level for Teeworlds 0.7 (0: Casual, 1: Normal, 2: Competitive)")

MACRO_CONFIG_STR(EcBindaddr, ec_bindaddr, 128, "localhost", CFGFLAG_ECON, "Address to bind the external console to. Anything but 'localhost' is dangerous")
MACRO_CONFIG_INT(EcPort, ec_port, 0, 0, 65535, CFGFLAG_ECON, "Port to use for the external console")
MACRO_CONFIG_STR(EcPassword, ec_password, 128, "", CFGFLAG_ECON, "External console password. This option is required to be set for econ to be enabled.")
MACRO_CONFIG_INT(EcBantime, ec_bantime, 0, 0, 1440, CFGFLAG_ECON, "The time a client gets banned if econ authentication fails. 0 just closes the connection")
MACRO_CONFIG_INT(EcAuthTimeout, ec_auth_timeout, 30, 1, 120, CFGFLAG_ECON, "Time in seconds before the the econ authentication times out")
MACRO_CONFIG_INT(EcOutputLevel, ec_output_level, 0, -3, 2, CFGFLAG_ECON, "Adjusts the amount of information in the external console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")

MACRO_CONFIG_INT(Debug, debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug mode")
MACRO_CONFIG_INT(DbgSql, dbg_sql, 1, 0, 1, CFGFLAG_SERVER, "Debug SQL")
MACRO_CONFIG_INT(DbgCurl, dbg_curl, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug curl")
MACRO_CONFIG_INT(DbgGraphs, dbg_graphs, 0, 0, 1, CFGFLAG_CLIENT, "Performance graphs")
MACRO_CONFIG_INT(DbgGfx, dbg_gfx, 0, 0, 4, CFGFLAG_CLIENT, "Show graphic library warnings and errors, if the GPU supports it (0: none, 1: minimal, 2: affects performance, 3: verbose, 4: all)")
#ifdef CONF_DEBUG
MACRO_CONFIG_INT(DbgStress, dbg_stress, 0, 0, 1, CFGFLAG_CLIENT, "Stress systems (Debug build only)")
MACRO_CONFIG_STR(DbgStressServer, dbg_stress_server, 32, "localhost", CFGFLAG_CLIENT, "Server to stress (Debug build only)")
#endif

MACRO_CONFIG_INT(HttpAllowInsecure, http_allow_insecure, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Allow insecure HTTP protocol in addition to the secure HTTPS one. Mostly useful for testing.")

// DDRace
MACRO_CONFIG_STR(SvWelcome, sv_welcome, 256, "", CFGFLAG_SERVER, "Message that will be displayed to players who join the server")
MACRO_CONFIG_INT(SvReservedSlots, sv_reserved_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "The number of slots that are reserved for special players")
MACRO_CONFIG_STR(SvReservedSlotsPass, sv_reserved_slots_pass, 256, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "The password that is required to use a reserved slot")
MACRO_CONFIG_INT(SvReservedSlotsAuthLevel, sv_reserved_slots_auth_level, 1, 1, 4, CFGFLAG_SERVER, "Minimum rcon auth level needed to use a reserved slot. 4 = rcon auth disabled")
MACRO_CONFIG_INT(SvHit, sv_hit, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether players can hammer/grenade/laser each other or not")
MACRO_CONFIG_INT(SvEndlessDrag, sv_endless_drag, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Turns endless hooking on/off")
MACRO_CONFIG_INT(SvTestingCommands, sv_test_cmds, 0, 0, 1, CFGFLAG_SERVER, "Turns testing commands aka cheats on/off (setting only works in initial config)")
MACRO_CONFIG_INT(SvFreezeDelay, sv_freeze_delay, 3, 1, 30, CFGFLAG_SERVER | CFGFLAG_GAME, "How many seconds the players will remain frozen (applies to all except delayed freeze in switch layer & deepfreeze)")
MACRO_CONFIG_INT(ClDDRaceBindsSet, cl_race_binds_set, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What level the DDRace binds are set to (this is automated, you don't need to use this)")
MACRO_CONFIG_INT(SvEndlessSuperHook, sv_endless_super_hook, 0, 0, 1, CFGFLAG_SERVER, "Endless hook for super players on/off")
MACRO_CONFIG_INT(SvHideScore, sv_hide_score, 0, 0, 1, CFGFLAG_SERVER, "Whether players scores will be announced or not")
MACRO_CONFIG_INT(SvSaveWorseScores, sv_save_worse_scores, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to save worse scores when you already have a better one")
MACRO_CONFIG_INT(SvPauseable, sv_pauseable, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether players can pause their char or not")
MACRO_CONFIG_INT(SvPauseMessages, sv_pause_messages, 0, 0, 1, CFGFLAG_SERVER, "Whether to show messages when a player pauses and resumes")
MACRO_CONFIG_INT(SvSpecFrequency, sv_pause_frequency, 1, 0, 9999, CFGFLAG_SERVER, "The minimum allowed delay between /spec")
MACRO_CONFIG_INT(SvInvite, sv_invite, 1, 0, 1, CFGFLAG_SERVER, "Whether players can invite other players to teams")
MACRO_CONFIG_INT(SvInviteFrequency, sv_invite_frequency, 1, 0, 9999, CFGFLAG_SERVER, "The minimum allowed delay between invites")
MACRO_CONFIG_INT(SvTeleOthersAuthLevel, sv_tele_others_auth_level, 1, 1, 3, CFGFLAG_SERVER, "The auth level you need to tele others")
MACRO_CONFIG_INT(SvRegionalRankings, sv_regional_rankings, 1, 0, 1, CFGFLAG_SERVER, "Display regional rankings in /rank, /top5 and /top5team")

MACRO_CONFIG_INT(SvEmotionalTees, sv_emotional_tees, 1, -1, 1, CFGFLAG_SERVER, "Whether eye change of tees is enabled with emoticons = 1, not = 0, -1 not at all")
MACRO_CONFIG_INT(SvEmoticonMsDelay, sv_emoticon_ms_delay, 3000, 20, 999999999, CFGFLAG_SERVER, "The time in ms a player has to wait before allowing the next over-head emoticons")
MACRO_CONFIG_INT(SvGlobalEmoticonMsDelay, sv_global_emoticon_ms_delay, 3000, 20, 999999999, CFGFLAG_SERVER, "The time in ms a player has to wait before allowing the next over-head emoticons that is send to all clients (this config must be higher or equal to sv_emoticon_ms_delay to have an effect)")
MACRO_CONFIG_INT(SvEyeEmoteChangeDelay, sv_eye_emote_change_delay, 1, 0, 9999, CFGFLAG_SERVER, "The time in seconds between eye emoticons change")

MACRO_CONFIG_INT(SvChatDelay, sv_chat_delay, 1, 0, 9999, CFGFLAG_SERVER, "The time in seconds between chat messages")
MACRO_CONFIG_INT(SvTeamChangeDelay, sv_team_change_delay, 3, 0, 9999, CFGFLAG_SERVER, "The time in seconds between team changes (spectator/in game)")
MACRO_CONFIG_INT(SvInfoChangeDelay, sv_info_change_delay, 5, 0, 9999, CFGFLAG_SERVER, "The time in seconds between info changes (name/skin/color), to avoid ranbow mod set this to a very high time")
MACRO_CONFIG_INT(SvVoteTime, sv_vote_time, 25, 1, 60, CFGFLAG_SERVER, "The time in seconds a vote lasts")
MACRO_CONFIG_INT(SvVoteMapTimeDelay, sv_vote_map_delay, 0, 0, 9999, CFGFLAG_SERVER, "The minimum time in seconds between map votes")
MACRO_CONFIG_INT(SvVoteDelay, sv_vote_delay, 3, 0, 9999, CFGFLAG_SERVER, "The time in seconds between any vote")
MACRO_CONFIG_INT(SvVoteKickDelay, sv_vote_kick_delay, 0, 0, 9999, CFGFLAG_SERVER, "The minimum time in seconds between kick votes")
MACRO_CONFIG_INT(SvVoteYesPercentage, sv_vote_yes_percentage, 50, 1, 99, CFGFLAG_SERVER, "More than this percentage of players need to agree for a vote to succeed")
MACRO_CONFIG_INT(SvVoteMajority, sv_vote_majority, 0, 0, 1, CFGFLAG_SERVER, "Whether non-voting players are considered as votes for \"no\" (0) or are ignored (1)")
MACRO_CONFIG_INT(SvVoteMaxTotal, sv_vote_max_total, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "How many players can participate in a vote at max (0 = no limit)")
MACRO_CONFIG_INT(SvVoteVetoTime, sv_vote_veto_time, 20, 0, 1000, CFGFLAG_SERVER, "Minutes of time on a server until a player can veto map change votes (0 = disabled)")
MACRO_CONFIG_INT(SvKillDelay, sv_kill_delay, 1, 0, 9999, CFGFLAG_SERVER, "The minimum time in seconds between kills")

MACRO_CONFIG_INT(SvMapWindow, sv_map_window, 15, 0, 100, CFGFLAG_SERVER, "Map downloading send-ahead window")
MACRO_CONFIG_INT(SvFastDownload, sv_fast_download, 1, 0, 1, CFGFLAG_SERVER, "Enables fast download of maps")

MACRO_CONFIG_INT(SvShotgunBulletSound, sv_shotgun_bullet_sound, 0, 0, 1, CFGFLAG_SERVER, "Crazy shotgun bullet sound on/off")

MACRO_CONFIG_STR(SvRegionName, sv_region_name, 5, "UNK", CFGFLAG_SERVER, "Server region. Used for regional bans")
MACRO_CONFIG_STR(SvSqlServerName, sv_sql_servername, 5, "UNK", CFGFLAG_SERVER, "SQL Server name that is inserted into record table")
MACRO_CONFIG_INT(SvSaveGames, sv_savegames, 1, 0, 1, CFGFLAG_SERVER, "Enables savegames (/save and /load)")
MACRO_CONFIG_INT(SvSaveSwapGamesDelay, sv_saveswapgames_delay, 30, 0, 10000, CFGFLAG_SERVER, "Delay in seconds for loading a savegame or before swapping")
MACRO_CONFIG_INT(SvSaveSwapGamesPenalty, sv_saveswapgames_penalty, 60, 0, 10000, CFGFLAG_SERVER, "Penalty in seconds for saving or swapping position")
MACRO_CONFIG_INT(SvSwapTimeout, sv_swap_timeout, 180, 0, 10000, CFGFLAG_SERVER, "Timeout in seconds before option to swap expires")
MACRO_CONFIG_INT(SvSwap, sv_swap, 1, 0, 1, CFGFLAG_SERVER, "Enable /swap")
MACRO_CONFIG_INT(SvTeam0Mode, sv_team0mode, 1, 0, 1, CFGFLAG_SERVER, "Enables /team0mode")
MACRO_CONFIG_INT(SvUseSql, sv_use_sql, 0, 0, 1, CFGFLAG_SERVER, "Enables MySQL backend instead of SQLite backend (sv_sqlite_file is still used as fallback write server when no MySQL server is reachable)")
MACRO_CONFIG_INT(SvSqlQueriesDelay, sv_sql_queries_delay, 1, 0, 20, CFGFLAG_SERVER, "Delay in seconds between SQL queries of a single player")
MACRO_CONFIG_STR(SvSqliteFile, sv_sqlite_file, 64, "ddnet-server.sqlite", CFGFLAG_SERVER, "File to store ranks in case sv_use_sql is turned off or used as backup sql server")

#if defined(CONF_UPNP)
MACRO_CONFIG_INT(SvUseUPnP, sv_use_upnp, 0, 0, 1, CFGFLAG_SERVER, "Enables UPnP support. (Requires -DCONF_UPNP=ON when compiling)")
#endif

MACRO_CONFIG_INT(SvDDRaceRules, sv_ddrace_rules, 1, 0, 1, CFGFLAG_SERVER, "Whether the default mod rules are displayed or not")
MACRO_CONFIG_STR(SvRulesLine1, sv_rules_line1, 128, "", CFGFLAG_SERVER, "Rules line 1")
MACRO_CONFIG_STR(SvRulesLine2, sv_rules_line2, 128, "", CFGFLAG_SERVER, "Rules line 2")
MACRO_CONFIG_STR(SvRulesLine3, sv_rules_line3, 128, "", CFGFLAG_SERVER, "Rules line 3")
MACRO_CONFIG_STR(SvRulesLine4, sv_rules_line4, 128, "", CFGFLAG_SERVER, "Rules line 4")
MACRO_CONFIG_STR(SvRulesLine5, sv_rules_line5, 128, "", CFGFLAG_SERVER, "Rules line 5")
MACRO_CONFIG_STR(SvRulesLine6, sv_rules_line6, 128, "", CFGFLAG_SERVER, "Rules line 6")
MACRO_CONFIG_STR(SvRulesLine7, sv_rules_line7, 128, "", CFGFLAG_SERVER, "Rules line 7")
MACRO_CONFIG_STR(SvRulesLine8, sv_rules_line8, 128, "", CFGFLAG_SERVER, "Rules line 8")
MACRO_CONFIG_STR(SvRulesLine9, sv_rules_line9, 128, "", CFGFLAG_SERVER, "Rules line 9")
MACRO_CONFIG_STR(SvRulesLine10, sv_rules_line10, 128, "", CFGFLAG_SERVER, "Rules line 10")

MACRO_CONFIG_INT(SvTeam, sv_team, 1, 0, 3, CFGFLAG_SERVER | CFGFLAG_GAME, "Teams configuration (0 = off, 1 = on but optional, 2 = must play only with teams, 3 = forced random team only for you)")
MACRO_CONFIG_INT(SvMinTeamSize, sv_min_team_size, 2, 1, MAX_CLIENTS, CFGFLAG_SERVER | CFGFLAG_GAME, "Minimum team size (finishing in a team smaller than this size gives you no teamrank)")
MACRO_CONFIG_INT(SvMaxTeamSize, sv_max_team_size, MAX_CLIENTS, 1, MAX_CLIENTS, CFGFLAG_SERVER | CFGFLAG_GAME, "Maximum team size")
MACRO_CONFIG_INT(SvMapVote, sv_map_vote, 1, 0, 1, CFGFLAG_SERVER, "Whether to allow /map")

MACRO_CONFIG_STR(SvAnnouncementFileName, sv_announcement_filename, IO_MAX_PATH_LENGTH, "announcement.txt", CFGFLAG_SERVER, "File which contains the announcements, one on each line")
MACRO_CONFIG_INT(SvAnnouncementInterval, sv_announcement_interval, 120, 1, 9999, CFGFLAG_SERVER, "The time (minutes) for how often an announcement will be displayed from the announcement file")
MACRO_CONFIG_INT(SvAnnouncementRandom, sv_announcement_random, 1, 0, 1, CFGFLAG_SERVER, "Whether announcements are sequential or random")

MACRO_CONFIG_INT(SvOldLaser, sv_old_laser, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether lasers can hit you if you shot them and that they pull you towards the bounce origin (0 for all new maps) or lasers can't hit you if you shot them, and they pull others towards the shooter")
MACRO_CONFIG_INT(SvSlashMe, sv_slash_me, 0, 0, 1, CFGFLAG_SERVER, "Whether /me is active on the server or not")
MACRO_CONFIG_INT(SvRejoinTeam0, sv_rejoin_team_0, 1, 0, 1, CFGFLAG_SERVER, "Make a team automatically rejoin team 0 after finish (only if not locked)")

MACRO_CONFIG_INT(SvNoWeakHook, sv_no_weak_hook, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to use an alternative calculation for world ticks, that makes the hook behave like all players have strong.")

MACRO_CONFIG_INT(ClReconnectTimeout, cl_reconnect_timeout, 120, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many seconds to wait before reconnecting (after timeout, 0 for off)")
MACRO_CONFIG_INT(ClReconnectFull, cl_reconnect_full, 5, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many seconds to wait before reconnecting (when server is full, 0 for off)")

MACRO_CONFIG_COL(ClMessageSystemColor, cl_message_system_color, 2817983, CFGFLAG_CLIENT | CFGFLAG_SAVE, "System message color")
MACRO_CONFIG_COL(ClMessageClientColor, cl_message_client_color, 9633471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color")
MACRO_CONFIG_COL(ClMessageHighlightColor, cl_message_highlight_color, 65471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Highlighted message color")
MACRO_CONFIG_COL(ClMessageTeamColor, cl_message_team_color, 5636050, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Team message color")
MACRO_CONFIG_COL(ClMessageColor, cl_message_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message color")
MACRO_CONFIG_COL(ClLaserRifleInnerColor, cl_laser_rifle_inner_color, 11206591, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for Rifle")
MACRO_CONFIG_COL(ClLaserRifleOutlineColor, cl_laser_rifle_outline_color, 11176233, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for Rifle")
MACRO_CONFIG_COL(ClLaserShotgunInnerColor, cl_laser_sg_inner_color, 1467241, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for Shotgun")
MACRO_CONFIG_COL(ClLaserShotgunOutlineColor, cl_laser_sg_outline_color, 1866773, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for Shotgun")
MACRO_CONFIG_COL(ClLaserDoorInnerColor, cl_laser_door_inner_color, 7701379, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for doors")
MACRO_CONFIG_COL(ClLaserDoorOutlineColor, cl_laser_door_outline_color, 7667473, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for doors")
MACRO_CONFIG_COL(ClLaserFreezeInnerColor, cl_laser_freeze_inner_color, 12001153, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for freezes")
MACRO_CONFIG_COL(ClLaserFreezeOutlineColor, cl_laser_freeze_outline_color, 11613223, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for freezes")
MACRO_CONFIG_COL(ClKillMessageNormalColor, cl_kill_message_normal_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Kill message normal color")
MACRO_CONFIG_COL(ClKillMessageHighlightColor, cl_kill_message_highlight_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Kill message highlight color")

MACRO_CONFIG_INT(ClMessageFriend, cl_message_friend, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable coloring and the heart for friends")
MACRO_CONFIG_COL(ClMessageFriendColor, cl_message_friend_color, 65425, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Friend message color")

MACRO_CONFIG_INT(ConnTimeout, conn_timeout, 100, 5, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Network timeout")
MACRO_CONFIG_INT(ConnTimeoutProtection, conn_timeout_protection, 1000, 5, 10000, CFGFLAG_SERVER, "Network timeout protection")
MACRO_CONFIG_INT(ClShowIds, cl_show_ids, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to show client IDs in scoreboard, chat and spectator menu")
MACRO_CONFIG_INT(ClScoreboardOnDeath, cl_scoreboard_on_death, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to show scoreboard after death or not")
MACRO_CONFIG_INT(ClAutoRaceRecord, cl_auto_race_record, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save the best demo of each race")
MACRO_CONFIG_INT(ClReplays, cl_replays, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable/disable replays")
MACRO_CONFIG_INT(ClReplayLength, cl_replay_length, 30, 10, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set the default length of the replays")
MACRO_CONFIG_INT(ClRaceRecordServerControl, cl_race_record_server_control, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let the server start the race recorder")
MACRO_CONFIG_INT(ClDemoName, cl_demo_name, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save the player name within the demo")
MACRO_CONFIG_INT(ClRaceGhost, cl_race_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable ghost")
MACRO_CONFIG_INT(ClRaceGhostServerControl, cl_race_ghost_server_control, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let the server start the ghost")
MACRO_CONFIG_INT(ClRaceShowGhost, cl_race_show_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghost")
MACRO_CONFIG_INT(ClRaceSaveGhost, cl_race_save_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save ghost")
MACRO_CONFIG_INT(ClRaceGhostStrictMap, cl_race_ghost_strict_map, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Match ghosts by map version instead of only map name")
MACRO_CONFIG_INT(ClRaceGhostSaveBest, cl_race_ghost_save_best, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save only ghosts that are better than the previous record.")
MACRO_CONFIG_INT(ClRaceGhostAlpha, cl_race_ghost_alpha, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Visbility of ghosts (alpha value, 0 invisible, 100 fully visible)")

MACRO_CONFIG_INT(SvResetPickups, sv_reset_pickups, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether the weapons are reset on passing the start tile or not")
MACRO_CONFIG_INT(ClShowOthers, cl_show_others, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players in other teams (2 to show own team only)")
MACRO_CONFIG_INT(ClShowOthersAlpha, cl_show_others_alpha, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players in other teams (alpha value, 0 invisible, 100 fully visible)")
MACRO_CONFIG_INT(ClOverlayEntities, cl_overlay_entities, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Overlay game tiles with a percentage of opacity")
MACRO_CONFIG_INT(ClShowQuads, cl_showquads, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show quads (only interesting for mappers, or if your system has extremely bad performance)")
MACRO_CONFIG_COL(ClBackgroundColor, cl_background_color, 128, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background color") // 0 0 128
MACRO_CONFIG_COL(ClBackgroundEntitiesColor, cl_background_entities_color, 128, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background (entities) color") // 0 0 128
MACRO_CONFIG_STR(ClBackgroundEntities, cl_background_entities, IO_MAX_PATH_LENGTH, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background (entities)")
MACRO_CONFIG_STR(ClRunOnJoin, cl_run_on_join, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Command to run when joining a server")

// menu background map
MACRO_CONFIG_STR(ClMenuMap, cl_menu_map, 100, "auto", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background map in the menu")
MACRO_CONFIG_INT(ClRotationRadius, cl_rotation_radius, 30, 1, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Menu camera rotation radius")
MACRO_CONFIG_INT(ClRotationSpeed, cl_rotation_speed, 40, 1, 120, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Menu camera rotations in seconds")
MACRO_CONFIG_INT(ClCameraSpeed, cl_camera_speed, 5, 1, 40, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Menu camera speed")

MACRO_CONFIG_INT(ClBackgroundShowTilesLayers, cl_background_show_tiles_layers, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether draw tiles layers when using custom background (entities)")
MACRO_CONFIG_INT(SvShowOthers, sv_show_others, 1, 0, 1, CFGFLAG_SERVER, "Whether players can use the command showothers or not")
MACRO_CONFIG_INT(SvShowOthersDefault, sv_show_others_default, 0, 0, 2, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether players see others by default (2 for own team)")
MACRO_CONFIG_INT(SvShowAllDefault, sv_show_all_default, 0, 0, 1, CFGFLAG_SERVER, "Whether players see all tees by default")
MACRO_CONFIG_INT(SvMaxAfkTime, sv_max_afk_time, 300, 0, 9999, CFGFLAG_SERVER, "The time in seconds a player to be afk (0 = disabled)")
MACRO_CONFIG_INT(SvPlasmaRange, sv_plasma_range, 700, 1, 99999, CFGFLAG_SERVER | CFGFLAG_GAME, "How far will the plasma gun track tees")
MACRO_CONFIG_INT(SvPlasmaPerSec, sv_plasma_per_sec, 3, 0, 50, CFGFLAG_SERVER | CFGFLAG_GAME, "How many shots does the plasma gun fire per seconds")
MACRO_CONFIG_INT(SvDraggerRange, sv_dragger_range, 700, 1, 99999, CFGFLAG_SERVER | CFGFLAG_GAME, "How far will the dragger track tees")
MACRO_CONFIG_INT(SvVotePause, sv_vote_pause, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to pause players (instead of moving to spectators)")
MACRO_CONFIG_INT(SvVotePauseTime, sv_vote_pause_time, 10, 0, 360, CFGFLAG_SERVER, "The time (in seconds) players have to wait in pause when paused by vote")
MACRO_CONFIG_INT(SvTuneReset, sv_tune_reset, 1, 0, 1, CFGFLAG_SERVER, "Whether tuning is reset after each map change or not")
MACRO_CONFIG_STR(SvResetFile, sv_reset_file, 128, "reset.cfg", CFGFLAG_SERVER, "File to execute on map change or reload to set the default server settings")
MACRO_CONFIG_STR(SvInputFifo, sv_input_fifo, 128, "", CFGFLAG_SERVER, "Fifo file (non-Windows) or Named Pipe (Windows) to use as input for server console")
MACRO_CONFIG_INT(SvDDRaceTuneReset, sv_ddrace_tune_reset, 1, 0, 1, CFGFLAG_SERVER, "Whether DDRace tuning (sv_hit, sv_endless_drag and sv_old_laser) is reset after each map change or not")
MACRO_CONFIG_INT(SvNamelessScore, sv_nameless_score, 1, 0, 1, CFGFLAG_SERVER, "Whether nameless tee has a score or not")
MACRO_CONFIG_INT(SvTimeInBroadcastInterval, sv_time_in_broadcast_interval, 1, 0, 60, CFGFLAG_SERVER, "How often to update the broadcast time")
MACRO_CONFIG_INT(SvDefaultTimerType, sv_default_timer_type, 0, 0, 3, CFGFLAG_SERVER, "Default way of displaying time either game/round timer or broadcast. 0 = game/round timer, 1 = broadcast, 2 = 0+1, 3 = none")

// these might need some fine tuning
MACRO_CONFIG_INT(SvChatInitialDelay, sv_chat_initial_delay, 0, 0, 360, CFGFLAG_SERVER, "The time in seconds before the first message can be sent")
MACRO_CONFIG_INT(SvChatPenalty, sv_chat_penalty, 250, 50, 1000, CFGFLAG_SERVER, "chat score will be increased by this on every message, and decremented by 1 on every tick.")
MACRO_CONFIG_INT(SvChatThreshold, sv_chat_threshold, 1000, 50, 10000, CFGFLAG_SERVER, "if chats score exceeds this, the player will be muted for sv_spam_mute_duration seconds")
MACRO_CONFIG_INT(SvSpamMuteDuration, sv_spam_mute_duration, 60, 0, 3600, CFGFLAG_SERVER, "how many seconds to mute, if player triggers mute on spam. 0 = off")

MACRO_CONFIG_INT(SvShutdownWhenEmpty, sv_shutdown_when_empty, 0, 0, 1, CFGFLAG_SERVER, "Shutdown server as soon as no one is on it anymore")
MACRO_CONFIG_INT(SvReloadWhenEmpty, sv_reload_when_empty, 0, 0, 2, CFGFLAG_SERVER, "Reload map when server is empty (1 = reload once, 2 = reload every time server gets empty)")
MACRO_CONFIG_INT(SvKillProtection, sv_kill_protection, 20, 0, 9999, CFGFLAG_SERVER, "0 - Disable, 1-9999 minutes")
MACRO_CONFIG_INT(SvSoloServer, sv_solo_server, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Set server to solo mode (no player interactions, has to be set before loading the map)")
MACRO_CONFIG_STR(SvClientSuggestion, sv_client_suggestion, 128, "Get DDNet client from DDNet.org to use all features on DDNet!", CFGFLAG_SERVER, "Broadcast to display to players without DDNet client")
MACRO_CONFIG_STR(SvClientSuggestionOld, sv_client_suggestion_old, 128, "Your DDNet client is old, update it on DDNet.org!", CFGFLAG_SERVER, "Broadcast to display to players with an old version of DDNet client")
MACRO_CONFIG_STR(SvClientSuggestionBot, sv_client_suggestion_bot, 128, "Your client has bots and can be remotely controlled!\nPlease use another client like DDNet client from DDNet.org", CFGFLAG_SERVER, "Broadcast to display to players with a known botting client")
MACRO_CONFIG_STR(SvBannedVersions, sv_banned_versions, 128, "", CFGFLAG_SERVER, "Comma separated list of banned clients to be kicked on join")

// netlimit
MACRO_CONFIG_INT(SvNetlimit, sv_netlimit, 0, 0, 10000, CFGFLAG_SERVER, "Netlimit: Maximum amount of traffic a client is allowed to use (in kb/s)")
MACRO_CONFIG_INT(SvNetlimitAlpha, sv_netlimit_alpha, 50, 1, 100, CFGFLAG_SERVER, "Netlimit: Alpha of Exponention moving average")

MACRO_CONFIG_INT(SvConnlimit, sv_connlimit, 5, 0, 100, CFGFLAG_SERVER, "Connlimit: Number of connections an IP is allowed to do in a timespan")
MACRO_CONFIG_INT(SvConnlimitTime, sv_connlimit_time, 20, 0, 1000, CFGFLAG_SERVER, "Connlimit: Time in which IP's connections are counted")

#if defined(CONF_FAMILY_UNIX)
MACRO_CONFIG_STR(SvConnLoggingServer, sv_conn_logging_server, 128, "", CFGFLAG_SERVER, "Unix socket server for IP address logging (Unix only)")
#endif

MACRO_CONFIG_INT(ClUnpredictedShadow, cl_unpredicted_shadow, 0, -1, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show unpredicted shadow tee (0 = off, 1 = on, -1 = don't even show in debug mode)")
MACRO_CONFIG_INT(ClPredictFreeze, cl_predict_freeze, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict freeze tiles (0 = off, 1 = on, 2 = partial (allow a small amount of movement in freeze)")
MACRO_CONFIG_INT(ClShowNinja, cl_show_ninja, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ninja skin")
MACRO_CONFIG_INT(ClShowHookCollOther, cl_show_hook_coll_other, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show other players' hook collision line (2 to always show)")
MACRO_CONFIG_INT(ClShowHookCollOwn, cl_show_hook_coll_own, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show own players' hook collision line (2 to always show)")
MACRO_CONFIG_INT(ClHookCollSize, cl_hook_coll_size, 0, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of your own hook collision line")
MACRO_CONFIG_INT(ClHookCollSizeOther, cl_hook_coll_size_other, 0, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of others' hook collision line")
MACRO_CONFIG_INT(ClHookCollAlpha, cl_hook_coll_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of hook collision line (0 invisible, 100 fully visible)")

MACRO_CONFIG_COL(ClHookCollColorNoColl, cl_hook_coll_color_no_coll, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies the color of a hookline that hits nothing.")
MACRO_CONFIG_COL(ClHookCollColorHookableColl, cl_hook_coll_color_hookable_coll, 6401973, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies the color of a hookline that hits hookable tiles.")
MACRO_CONFIG_COL(ClHookCollColorTeeColl, cl_hook_coll_color_tee_coll, 2817919, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies the color of a hookline that hits tees.")

MACRO_CONFIG_INT(ClChatTeamColors, cl_chat_teamcolors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show names in chat in team colors")
MACRO_CONFIG_INT(ClChatReset, cl_chat_reset, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset chat when pressing escape")
MACRO_CONFIG_INT(ClChatOld, cl_chat_old, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Old chat style: No tee, no background")
MACRO_CONFIG_INT(ClChatFontSize, cl_chat_size, 60, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Chat font size")
MACRO_CONFIG_INT(ClChatWidth, cl_chat_width, 200, 140, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Chat width")

MACRO_CONFIG_INT(ClShowDirection, cl_show_direction, 1, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show key presses (1 = other players', 2 = everyone, 3 = only your own")
MACRO_CONFIG_INT(ClDirectionSize, cl_direction_size, 30, -50, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Size of key press icons")
MACRO_CONFIG_INT(ClOldGunPosition, cl_old_gun_position, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Tees hold gun a bit higher like in TW 0.6.1 and older")
MACRO_CONFIG_INT(ClConfirmDisconnectTime, cl_confirm_disconnect_time, 20, -1, 1440, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Confirmation popup before disconnecting after game time (in minutes, -1 to turn off, 0 to always turn on)")
MACRO_CONFIG_INT(ClConfirmQuitTime, cl_confirm_quit_time, 20, -1, 1440, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Confirmation popup before quitting after game time (in minutes, -1 to turn off, 0 to always turn on)")
MACRO_CONFIG_STR(ClTimeoutCode, cl_timeout_code, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Timeout code to use")
MACRO_CONFIG_STR(ClDummyTimeoutCode, cl_dummy_timeout_code, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Dummy Timeout code to use")
MACRO_CONFIG_STR(ClTimeoutSeed, cl_timeout_seed, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Timeout seed")
MACRO_CONFIG_STR(ClInputFifo, cl_input_fifo, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Fifo file (non-Windows) or Named Pipe (Windows) to use as input for client console")
MACRO_CONFIG_INT(ClConfigVersion, cl_config_version, 0, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The config version. Helps newer clients fix bugs with older configs.")

// demo editor
MACRO_CONFIG_INT(ClDemoSliceBegin, cl_demo_slice_begin, -1, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Begin marker for demo slice")
MACRO_CONFIG_INT(ClDemoSliceEnd, cl_demo_slice_end, -1, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "End marker for demo slice")
MACRO_CONFIG_INT(ClDemoShowSpeed, cl_demo_show_speed, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show speed meter on change")
MACRO_CONFIG_INT(ClDemoShowPause, cl_demo_show_pause, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show pause/play indicator on change")
MACRO_CONFIG_INT(ClDemoKeyboardShortcuts, cl_demo_keyboard_shortcuts, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable keyboard shortcuts in demo player")

// graphic library
#if !defined(CONF_ARCH_IA32) && !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_INT(GfxGLMajor, gfx_gl_major, 1, 1, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library major version")
#elif !defined(CONF_ARCH_IA32)
MACRO_CONFIG_INT(GfxGLMajor, gfx_gl_major, 3, 1, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library major version")
#else
MACRO_CONFIG_INT(GfxGLMajor, gfx_gl_major, 1, 1, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library major version")
#endif
#if !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_INT(GfxGLMinor, gfx_gl_minor, 1, 0, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library minor version")
#else
MACRO_CONFIG_INT(GfxGLMinor, gfx_gl_minor, 3, 0, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library minor version")
#endif
MACRO_CONFIG_INT(GfxGLPatch, gfx_gl_patch, 0, 0, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library patch version")

// float multiplied with 1000
MACRO_CONFIG_INT(GfxGLTextureLODBIAS, gfx_gl_texture_lod_bias, -500, -15000, 15000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "The lod bias for graphic library texture sampling multiplied by 1000")

MACRO_CONFIG_INT(Gfx3DTextureAnalysisRan, gfx_3d_texture_analysis_ran, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Ran an analyzer to check if sampling 3D/2D array textures works correctly")
MACRO_CONFIG_STR(Gfx3DTextureAnalysisRenderer, gfx_3d_texture_analysis_renderer, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The renderer on which the analysis was performed")
MACRO_CONFIG_STR(Gfx3DTextureAnalysisVersion, gfx_3d_texture_analysis_version, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The version on which the analysis was performed")

MACRO_CONFIG_STR(GfxGpuName, gfx_gpu_name, 256, "auto", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The GPU's name, which will be selected by the backend. (if supported by the backend)")
#if defined(CONF_PLATFORM_ANDROID)
MACRO_CONFIG_STR(GfxBackend, gfx_backend, 256, "GLES", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The backend to use (e.g. GLES or Vulkan)")
#elif !defined(CONF_ARCH_IA32) && !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_STR(GfxBackend, gfx_backend, 256, "Vulkan", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The backend to use (e.g. OpenGL or Vulkan)")
#else
MACRO_CONFIG_STR(GfxBackend, gfx_backend, 256, "OpenGL", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The backend to use (e.g. OpenGL or Vulkan)")
#endif
MACRO_CONFIG_INT(GfxRenderThreadCount, gfx_render_thread_count, 3, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Number of threads the backend can use for rendering. (note: the value can be ignored by the backend)")

MACRO_CONFIG_INT(GfxDriverIsBlocked, gfx_driver_is_blocked, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "If 1, the current driver is in a blocked error state.")

MACRO_CONFIG_INT(ClVideoRecorderFPS, cl_video_recorder_fps, 60, 1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "At which FPS the videorecorder should record demos.")

/*
 * Add config variables for mods below this comment to avoid merge conflicts.
 */
