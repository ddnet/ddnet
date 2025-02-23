#ifndef GAME_VOTING_H
#define GAME_VOTING_H

enum
{
	VOTE_DESC_LENGTH = 64,
	VOTE_CMD_LENGTH = 512,
	VOTE_REASON_LENGTH = 16,

	MAX_VOTE_OPTIONS = 8192,
};

struct CVoteOptionClient
{
	CVoteOptionClient *m_pNext;
	CVoteOptionClient *m_pPrev;
	char m_aDescription[VOTE_DESC_LENGTH];
};

struct CVoteOptionServer
{
	CVoteOptionServer *m_pNext;
	CVoteOptionServer *m_pPrev;
	char m_aDescription[VOTE_DESC_LENGTH];
	char m_aCommand[1];
};

#endif
