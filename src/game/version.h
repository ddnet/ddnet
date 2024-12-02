/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VERSION_H
#define GAME_VERSION_H
#ifndef GAME_RELEASE_VERSION
#define GAME_RELEASE_VERSION "18.9"
#endif

// teeworlds
#define CLIENT_VERSION7 0x0705
#define GAME_VERSION "0.6.4, " GAME_RELEASE_VERSION
#define GAME_NETVERSION "0.6 626fce9a778df4d4"
#define GAME_NETVERSION7 "0.7 802f1be60a05665f"

// ddnet
#define DDNET_VERSION_NUMBER 18090
extern const char *GIT_SHORTREV_HASH;
#define GAME_NAME "DDNet"
#endif
