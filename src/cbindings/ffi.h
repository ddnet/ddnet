#pragma once

#include <cstdint>

#ifdef _WIN32
  #define PUB __declspec(dllexport)
#else
  #define PUB __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DDNET_ABI_VERSION 0

// library functions
PUB uint32_t ddnet_abi_version();
PUB const char* ddnet_commit();
PUB const char* ddnet_version();

// Rust bindings for the DdnetCpp struct
typedef struct DdnetBindings {
	void *m_pImpl;
} DdnetBindings;

// 10x int32_t
struct CNetObj_PlayerInput;

// World wrapper, returns non-zero value on error
PUB int32_t ddnet_init(DdnetBindings* Self, const char *pDirectory);
PUB void ddnet_free(DdnetBindings* Self);

// helper to dump all configs
PUB const char** dump_configs();

// Game trait
PUB void ddnet_player_join(DdnetBindings Self, int32_t Id);
PUB void ddnet_player_ready(DdnetBindings Self, int32_t Id);
PUB void ddnet_player_input(DdnetBindings Self, int32_t Id, const CNetObj_PlayerInput *pInput);
PUB void ddnet_player_leave(DdnetBindings Self, int32_t Id);

PUB void ddnet_player_net(DdnetBindings Self, int32_t Id, const unsigned char *pNetMsg, uint32_t Len);
PUB void ddnet_on_command(DdnetBindings Self, int32_t Id, const char *pCommand);

PUB void ddnet_swap_tees(DdnetBindings Self, int32_t Id1, int32_t Id2);

PUB bool ddnet_is_empty(DdnetBindings Self);
PUB void ddnet_tick(DdnetBindings Self, int32_t CurTime);

// GameValidator trait
PUB int32_t ddnet_max_tee_id(DdnetBindings Self);
PUB int32_t ddnet_player_team(DdnetBindings Self, int32_t Id);
PUB void ddnet_set_player_team(DdnetBindings Self, int32_t Id, int32_t Team);
PUB void ddnet_tee_pos(DdnetBindings Self, int32_t Id, bool *pHasPos, int32_t *pX, int32_t *pY);
PUB void ddnet_set_tee_pos(DdnetBindings Self, int32_t Id, bool HasPos, int32_t X, int32_t Y);

// Snapper trait
PUB void ddnet_snap(DdnetBindings Self, const uint8_t **ppData, uint32_t *pLen);

// dont include the ReplayerConfigure trait. It's handled on the rust side

#ifdef __cplusplus
}
#endif
