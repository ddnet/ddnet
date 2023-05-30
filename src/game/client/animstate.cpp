/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <game/generated/client_data.h>

#include "animstate.h"

static void AnimSeqEval(const CAnimSequence *pSeq, float Time, CAnimKeyframe *pFrame)
{
	if(pSeq->m_NumFrames == 0)
	{
		pFrame->m_Time = 0;
		pFrame->m_X = 0;
		pFrame->m_Y = 0;
		pFrame->m_Angle = 0;
	}
	else if(pSeq->m_NumFrames == 1)
	{
		*pFrame = pSeq->m_aFrames[0];
	}
	else
	{
		CAnimKeyframe *pFrame1 = nullptr;
		CAnimKeyframe *pFrame2 = nullptr;
		float Blend = 0.0f;

		// TODO: make this smarter.. binary search
		for(int i = 1; i < pSeq->m_NumFrames; i++)
		{
			if(pSeq->m_aFrames[i - 1].m_Time <= Time && pSeq->m_aFrames[i].m_Time >= Time)
			{
				pFrame1 = &pSeq->m_aFrames[i - 1];
				pFrame2 = &pSeq->m_aFrames[i];
				Blend = (Time - pFrame1->m_Time) / (pFrame2->m_Time - pFrame1->m_Time);
				break;
			}
		}

		if(pFrame1 != nullptr && pFrame2 != nullptr)
		{
			pFrame->m_Time = Time;
			pFrame->m_X = mix(pFrame1->m_X, pFrame2->m_X, Blend);
			pFrame->m_Y = mix(pFrame1->m_Y, pFrame2->m_Y, Blend);
			pFrame->m_Angle = mix(pFrame1->m_Angle, pFrame2->m_Angle, Blend);
		}
	}
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

void CAnimState::Set(CAnimation *pAnim, float Time)
{
	AnimSeqEval(&pAnim->m_Body, Time, &m_Body);
	AnimSeqEval(&pAnim->m_BackFoot, Time, &m_BackFoot);
	AnimSeqEval(&pAnim->m_FrontFoot, Time, &m_FrontFoot);
	AnimSeqEval(&pAnim->m_Attach, Time, &m_Attach);
}

void CAnimState::Add(CAnimation *pAnim, float Time, float Amount)
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
