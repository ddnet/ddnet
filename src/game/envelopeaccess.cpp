/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/vmath.h>

#include <engine/shared/datafile.h>
#include <engine/shared/map.h>
#include "envelopeaccess.h"
#include <game/mapitems.h>
#include <game/mapitems_ex.h>

int IEnvelopePointAccess::FindPointIndex(CFixedTime Time) const
{
	// binary search for the interval around Time
	int Low = 0;
	int High = NumPoints() - 2;
	int FoundIndex = -1;

	while(Low <= High)
	{
		int Mid = Low + (High - Low) / 2;
		const CEnvPoint *pMid = GetPoint(Mid);
		const CEnvPoint *pNext = GetPoint(Mid + 1);
		if(Time >= pMid->m_Time && Time < pNext->m_Time)
		{
			FoundIndex = Mid;
			break;
		}
		else if(Time < pMid->m_Time)
		{
			High = Mid - 1;
		}
		else
		{
			Low = Mid + 1;
		}
	}
	return FoundIndex;
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(CDataFileReader *pReader)
{
	bool FoundBezierEnvelope = false;
	int EnvStart, EnvNum;
	pReader->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	for(int EnvIndex = 0; EnvIndex < EnvNum; EnvIndex++)
	{
		CMapItemEnvelope *pEnvelope = static_cast<CMapItemEnvelope *>(pReader->GetItem(EnvStart + EnvIndex));
		if(pEnvelope->m_Version >= CMapItemEnvelope::VERSION_TEEWORLDS_BEZIER)
		{
			FoundBezierEnvelope = true;
			break;
		}
	}

	if(FoundBezierEnvelope)
	{
		m_pPoints = nullptr;
		m_pPointsBezier = nullptr;

		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPointsBezierUpstream = static_cast<CEnvPointBezier_upstream *>(pReader->GetItem(EnvPointStart));
		else
			m_pPointsBezierUpstream = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPointBezier_upstream);
	}
	else
	{
		int EnvPointStart, FakeEnvPointNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS, &EnvPointStart, &FakeEnvPointNum);
		if(FakeEnvPointNum > 0)
			m_pPoints = static_cast<CEnvPoint *>(pReader->GetItem(EnvPointStart));
		else
			m_pPoints = nullptr;

		m_NumPointsMax = pReader->GetItemSize(EnvPointStart) / sizeof(CEnvPoint);

		int EnvPointBezierStart, FakeEnvPointBezierNum;
		pReader->GetType(MAPITEMTYPE_ENVPOINTS_BEZIER, &EnvPointBezierStart, &FakeEnvPointBezierNum);
		const int NumPointsBezier = pReader->GetItemSize(EnvPointBezierStart) / sizeof(CEnvPointBezier);
		if(FakeEnvPointBezierNum > 0 && m_NumPointsMax == NumPointsBezier)
			m_pPointsBezier = static_cast<CEnvPointBezier *>(pReader->GetItem(EnvPointBezierStart));
		else
			m_pPointsBezier = nullptr;

		m_pPointsBezierUpstream = nullptr;
	}

	SetPointsRange(0, m_NumPointsMax);
}

CMapBasedEnvelopePointAccess::CMapBasedEnvelopePointAccess(IMap *pMap) :
	CMapBasedEnvelopePointAccess(static_cast<CMap *>(pMap)->GetReader())
{
}

void CMapBasedEnvelopePointAccess::SetPointsRange(int StartPoint, int NumPoints)
{
	m_StartPoint = std::clamp(StartPoint, 0, m_NumPointsMax);
	m_NumPoints = std::clamp(NumPoints, 0, maximum(m_NumPointsMax - StartPoint, 0));
}

int CMapBasedEnvelopePointAccess::StartPoint() const
{
	return m_StartPoint;
}

int CMapBasedEnvelopePointAccess::NumPoints() const
{
	return m_NumPoints;
}

int CMapBasedEnvelopePointAccess::NumPointsMax() const
{
	return m_NumPointsMax;
}

const CEnvPoint *CMapBasedEnvelopePointAccess::GetPoint(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPoints != nullptr)
		return &m_pPoints[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint];
	return nullptr;
}

const CEnvPointBezier *CMapBasedEnvelopePointAccess::GetBezier(int Index) const
{
	if(Index < 0 || Index >= m_NumPoints)
		return nullptr;
	if(m_pPointsBezier != nullptr)
		return &m_pPointsBezier[Index + m_StartPoint];
	if(m_pPointsBezierUpstream != nullptr)
		return &m_pPointsBezierUpstream[Index + m_StartPoint].m_Bezier;
	return nullptr;
}