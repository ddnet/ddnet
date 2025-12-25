#include "ffi.h"

#include "ddnet.h"

#include <base/system.h>
#include <game/version.h>

bool IsInterrupted()
{
	return false;
}

uint32_t ddnet_abi_version() {
	return DDNET_ABI_VERSION;
}

const char* ddnet_commit(void) {
	return GIT_SHORTREV_HASH;
}

const char* ddnet_version(void) {
	return GAME_RELEASE_VERSION;
}

namespace {
CServerLib* Lib(DdnetBindings Self)
{
	dbg_assert(Self.m_pImpl, "uninitialized");
	return static_cast<CServerLib *>(Self.m_pImpl);
}
} // namespace

int32_t ddnet_init(DdnetBindings* pSelf, const char *pDirectory) {
	dbg_assert(pDirectory, "Need to specify directory");
	pSelf->m_pImpl = nullptr;

	CServerLib *pLib = new CServerLib();
	if (int32_t rv = pLib->Init(pDirectory) != 0) {
		delete pLib;
		return rv;
	}

	pSelf->m_pImpl = pLib;
	return 0;
}

void ddnet_free(DdnetBindings* pSelf)
{
	dbg_assert(pSelf->m_pImpl, "Not initialized");
	auto* pLib = Lib(*pSelf);
	pSelf->m_pImpl = nullptr;
	delete pLib;
}

// ---------------------------------------------------------------------------
// Game trait
// ---------------------------------------------------------------------------

void ddnet_player_join(DdnetBindings Self, uint32_t Id)
{
	Lib(Self)->PlayerJoin(Id);
}

void ddnet_player_ready(DdnetBindings Self, uint32_t Id)
{
	Lib(Self)->PlayerReady(Id);
}

void ddnet_player_input(DdnetBindings Self, uint32_t Id, const CNetObj_PlayerInput *pInput)
{
	Lib(Self)->PlayerInput(Id, pInput);
}

void ddnet_player_leave(DdnetBindings Self, uint32_t Id)
{
	Lib(Self)->PlayerLeave(Id);
}


void ddnet_player_net(DdnetBindings Self, uint32_t Id, const unsigned char *pNetMsg, uint32_t Len)
{
	Lib(Self)->PlayerNet(Id, pNetMsg, Len);
}

void ddnet_on_command(DdnetBindings Self, uint32_t Id, const char *pCommand)
{
	Lib(Self)->OnCommand(Id, pCommand);
}

void ddnet_swap_tees(DdnetBindings Self, uint32_t Id1, uint32_t Id2)
{
	Lib(Self)->SwapTees(Id1, Id2);
}

bool ddnet_is_empty(DdnetBindings Self) {
	return false;
}

void ddnet_tick(DdnetBindings Self, int32_t CurTime)
{
	Lib(Self)->Tick(CurTime);
}

// ---------------------------------------------------------------------------
// GameValidator trait
// ---------------------------------------------------------------------------

uint32_t ddnet_max_tee_id(DdnetBindings Self)
{
	return Lib(Self)->MaxTeeId();
}

int32_t ddnet_player_team(DdnetBindings Self, uint32_t Id)
{
	return Lib(Self)->PlayerTeam(Id);
}

void ddnet_set_player_team(DdnetBindings Self, uint32_t Id, int32_t Team)
{
	Lib(Self)->SetPlayerTeam(Id, Team);
}

void ddnet_tee_pos(DdnetBindings Self, uint32_t Id, bool *pHasPos, int32_t *pX, int32_t *pY)
{
	Lib(Self)->TeePos(Id, pHasPos, pX, pY);
}

void ddnet_set_tee_pos(DdnetBindings Self, uint32_t Id, bool HasPos, int32_t X, int32_t Y)
{
	Lib(Self)->SetTeePos(Id, HasPos, X, Y);
}

// ---------------------------------------------------------------------------
// Snapper trait
// ---------------------------------------------------------------------------

void ddnet_snap(DdnetBindings Self, const uint8_t **ppData, uint32_t *pLen)
{
	Lib(Self)->Snap(ppData, pLen);
}
