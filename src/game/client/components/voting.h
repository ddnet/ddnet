/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_VOTING_H
#define GAME_CLIENT_COMPONENTS_VOTING_H

#include <engine/console.h>
#include <engine/shared/memheap.h>

#include <game/client/component.h>
#include <game/voting.h>

class CUIRect;

class CVoting : public CComponent
{
	CHeap m_Heap;

	static void ConCallvote(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);

	int64_t m_Opentime;
	int64_t m_Closetime;
	char m_aDescription[VOTE_DESC_LENGTH];
	char m_aReason[VOTE_REASON_LENGTH];
	int m_Voted;
	int m_Yes, m_No, m_Pass, m_Total;
	bool m_ReceivingOptions;

	void RemoveOption(const char *pDescription);
	void ClearOptions();
	void Callvote(const char *pType, const char *pValue, const char *pReason);

	void RenderBars(CUIRect Bars) const;

public:
	int m_NumVoteOptions;
	CVoteOptionClient *m_pFirst;
	CVoteOptionClient *m_pLast;

	CVoteOptionClient *m_pRecycleFirst;
	CVoteOptionClient *m_pRecycleLast;

	CVoting();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnReset() override;
	virtual void OnConsoleInit() override;
	virtual void OnMessage(int Msgtype, void *pRawMsg) override;

	void Render();

	void CallvoteSpectate(int ClientId, const char *pReason, bool ForceVote = false);
	void CallvoteKick(int ClientId, const char *pReason, bool ForceVote = false);
	void CallvoteOption(int OptionId, const char *pReason, bool ForceVote = false);
	void RemovevoteOption(int OptionId);
	void AddvoteOption(const char *pDescription, const char *pCommand);
	void AddOption(const char *pDescription);

	void Vote(int v); // -1 = no, 1 = yes

	int SecondsLeft() const;
	bool IsVoting() const { return m_Closetime != 0; }
	int TakenChoice() const { return m_Voted; }
	const char *VoteDescription() const { return m_aDescription; }
	const char *VoteReason() const { return m_aReason; }
	bool IsReceivingOptions() const { return m_ReceivingOptions; }
};

#endif
