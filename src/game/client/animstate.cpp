/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "animstate.h"

#include <base/math.h>

#include <generated/client_data.h>

static void AnimSeqEval(const CAnimSequence *pSeq, float Time, CAnimKeyframe *pFrame)
{
	if(pSeq->m_NumFrames == 0)
	{
		*pFrame = {0, 0, 0, 0};
		return;
	}
	if(pSeq->m_NumFrames == 1 || pSeq->m_aFrames[0].m_Time >= Time)
	{
		*pFrame = pSeq->m_aFrames[0];
		return;
	}
	if(pSeq->m_aFrames[pSeq->m_NumFrames - 1].m_Time <= Time)
	{
		*pFrame = pSeq->m_aFrames[pSeq->m_NumFrames - 1];
		return;
	}

	int Low = 0;
	int High = pSeq->m_NumFrames - 1;
	int Index = 0;

	while(Low <= High)
	{
		int Mid = Low + (High - Low) / 2;
		if(pSeq->m_aFrames[Mid].m_Time <= Time)
		{
			Index = Mid;
			Low = Mid + 1;
		}
		else
		{
			High = Mid - 1;
		}
	}

	const CAnimKeyframe *pFrame1 = &pSeq->m_aFrames[Index];
	const CAnimKeyframe *pFrame2 = &pSeq->m_aFrames[Index + 1];

	float Diff = pFrame2->m_Time - pFrame1->m_Time;
	float Blend = (Diff > 0.0f) ? (Time - pFrame1->m_Time) / Diff : 0.0f;

	pFrame->m_Time = Time;
	pFrame->m_X = mix(pFrame1->m_X, pFrame2->m_X, Blend);
	pFrame->m_Y = mix(pFrame1->m_Y, pFrame2->m_Y, Blend);
	pFrame->m_Angle = mix(pFrame1->m_Angle, pFrame2->m_Angle, Blend);
}

static void AnimAddKeyframe(CAnimKeyframe *pSeq, const CAnimKeyframe *pAdded, float Amount)
{
	// AnimSeqEval fills m_X for any case, clang-analyzer assumes going into the
	// final else branch with pSeq->m_NumFrames < 2, which is impossible.
	pSeq->m_X += pAdded->m_X * Amount; // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
	pSeq->m_Y += pAdded->m_Y * Amount;
	pSeq->m_Angle += pAdded->m_Angle * Amount;
}

void CAnimState::AnimAdd(CAnimState *pState, const CAnimState *pAdded, float Amount)
{
	AnimAddKeyframe(&pState->m_Body, pAdded->GetBody(), Amount);
	AnimAddKeyframe(&pState->m_BackFoot, pAdded->GetBackFoot(), Amount);
	AnimAddKeyframe(&pState->m_FrontFoot, pAdded->GetFrontFoot(), Amount);
	AnimAddKeyframe(&pState->m_Attach, pAdded->GetAttach(), Amount);
}

void CAnimState::Set(const CAnimation *pAnim, float Time)
{
	AnimSeqEval(&pAnim->m_Body, Time, &m_Body);
	AnimSeqEval(&pAnim->m_BackFoot, Time, &m_BackFoot);
	AnimSeqEval(&pAnim->m_FrontFoot, Time, &m_FrontFoot);
	AnimSeqEval(&pAnim->m_Attach, Time, &m_Attach);
}

void CAnimState::Add(const CAnimation *pAnim, float Time, float Amount)
{
	CAnimState Add;
	Add.Set(pAnim, Time);
	AnimAdd(this, &Add, Amount);
}

const CAnimState *CAnimState::GetIdle()
{
	static CAnimState s_State;
	static bool s_Init = true;

	if(s_Init)
	{
		s_State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0.0f);
		s_State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0.0f, 1.0f);
		s_Init = false;
	}

	return &s_State;
}
