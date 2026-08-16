/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VERSION_H
#define GAME_VERSION_H

#include <generated/git_revision.h>

// ddnet
#define GAME_NAME "DDNet"
#define DDNET_VERSION_NUMBER 20010
extern const char *GIT_SHORTREV_HASH;
// Set this to the version being tagged, e.g. `20.1-rc1` or `20.1`. In versions
// ending in `-dev` the `-dev` is replaced by the git revision hash.
#define GAME_RELEASE_VERSION_INTERNAL "20.1-dev"

// teeworlds
#define CLIENT_VERSION7 0x0705
// For compatibility with DDNet client 15.8 and older we need to include the prefix `0.6` in the version string
// because this was used for a "Compatible version" filter in the server browser.
#define GAME_VERSION "0.6, " GAME_RELEASE_VERSION
#define GAME_NETVERSION "0.6 626fce9a778df4d4"
#define GAME_NETVERSION7 "0.7 802f1be60a05665f"

#endif
