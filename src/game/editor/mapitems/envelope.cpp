#include "envelope.h"

#include "map.h"

#include <base/dbg.h>

#include <algorithm>
#include <chrono>
#include <limits>

using namespace std::chrono_literals;

CEnvelope::CEnvelopePointAccess::CEnvelopePointAccess(const std::vector<CEnvelopePointValues> *pvPoints)
{
	m_pvPoints = pvPoints;
}

int CEnvelope::CEnvelopePointAccess::NumPoints() const
{
	return m_pvPoints->size();
}

const CEnvPoint *CEnvelope::CEnvelopePointAccess::GetPoint(int Index) const
{
	if(Index < 0 || (size_t)Index >= m_pvPoints->size())
		return nullptr;
	return &m_pvPoints->at(Index);
}

const CEnvPointBezier *CEnvelope::CEnvelopePointAccess::GetBezier(int Index) const
{
	if(Index < 0 || (size_t)Index >= m_pvPoints->size())
		return nullptr;
	return &m_pvPoints->at(Index).m_Bezier;
}

CEnvelope::CEnvelope(CEditorMap *pMap, EType Type, std::uint64_t RetainedId) :
	CMapObject(pMap), m_PointsAccess(&m_vPoints)
{
	m_Id = RetainedId ? RetainedId : Map()->AllocateObjectId();
	m_Type = Type;
}

CEnvelope::CEnvelope(CEditorMap *pMap, int NumChannels) :
	CMapObject(pMap), m_PointsAccess(&m_vPoints)
{
	m_Id = Map()->AllocateObjectId();
	switch(NumChannels)
	{
	case 1:
		m_Type = EType::SOUND;
		break;
	case 3:
		m_Type = EType::POSITION;
		break;
	case 4:
		m_Type = EType::COLOR;
		break;
	default:
		dbg_assert_failed("invalid number of channels for envelope");
	}
}

void CEnvelope::Resort()
{
	std::sort(m_vPoints.begin(), m_vPoints.end());
}

std::pair<float, float> CEnvelope::GetValueRange(int ChannelMask)
{
	if(m_vPoints.empty() || (ChannelMask & ((1 << GetChannels()) - 1)) == 0)
	{
		return {0.0f, 1.0f};
	}
	float Top = -std::numeric_limits<float>::infinity();
	float Bottom = std::numeric_limits<float>::infinity();
	for(size_t PointIndex = 0; PointIndex < m_vPoints.size(); ++PointIndex)
	{
		const auto &Point = m_vPoints[PointIndex];
		for(int c = 0; c < GetChannels(); c++)
		{
			if(ChannelMask & (1 << c))
			{
				{
					// value handle
					const float v = fx2f(Point.m_aValues[c]);
					Top = std::max(Top, v);
					Bottom = std::min(Bottom, v);
				}

				if(PointIndex < m_vPoints.size() - 1 && Point.m_Curvetype == CURVETYPE_BEZIER)
				{
					// out-tangent handle
					const float v = fx2f(Point.m_aValues[c] + Point.m_Bezier.m_aOutTangentDeltaY[c]);
					Top = std::max(Top, v);
					Bottom = std::min(Bottom, v);
				}

				if(PointIndex > 0 && m_vPoints[PointIndex - 1].m_Curvetype == CURVETYPE_BEZIER)
				{
					// in-tangent handle
					const float v = fx2f(Point.m_aValues[c] + Point.m_Bezier.m_aInTangentDeltaY[c]);
					Top = std::max(Top, v);
					Bottom = std::min(Bottom, v);
				}
			}
		}
	}
	return {Bottom, Top};
}

void CEnvelope::Eval(float Time, ColorRGBA &Result, size_t Channels) const
{
	Channels = std::min({Channels, (size_t)GetChannels(), (size_t)CEnvPoint::MAX_CHANNELS});
	CRenderMap::RenderEvalEnvelope(&m_PointsAccess, std::chrono::nanoseconds((int64_t)((double)Time * (double)std::chrono::nanoseconds(1s).count())), Result, Channels);
}

void CEnvelope::AddPoint(CFixedTime Time, std::array<int, CEnvPoint::MAX_CHANNELS> aValues)
{
	CEnvelopePointValues Point;
	Point.m_Id = Map()->AllocateObjectId();
	Point.m_Time = Time;
	Point.m_Curvetype = CURVETYPE_LINEAR;
	std::copy_n(aValues.begin(), std::size(Point.m_aValues), Point.m_aValues);
	std::fill(std::begin(Point.m_Bezier.m_aInTangentDeltaX), std::end(Point.m_Bezier.m_aInTangentDeltaX), CFixedTime(0));
	std::fill(std::begin(Point.m_Bezier.m_aInTangentDeltaY), std::end(Point.m_Bezier.m_aInTangentDeltaY), 0);
	std::fill(std::begin(Point.m_Bezier.m_aOutTangentDeltaX), std::end(Point.m_Bezier.m_aOutTangentDeltaX), CFixedTime(0));
	std::fill(std::begin(Point.m_Bezier.m_aOutTangentDeltaY), std::end(Point.m_Bezier.m_aOutTangentDeltaY), 0);
	m_vPoints.emplace_back(Point);
	Resort();
}

float CEnvelope::EndTime() const
{
	if(m_vPoints.empty())
		return 0.0f;
	return m_vPoints.back().m_Time.AsSeconds();
}

int CEnvelope::FindPointIndex(CFixedTime Time) const
{
	return m_PointsAccess.FindPointIndex(Time);
}

int CEnvelope::GetChannels() const
{
	switch(m_Type)
	{
	case EType::POSITION:
		return 3;
	case EType::COLOR:
		return 4;
	case EType::SOUND:
		return 1;
	default:
		dbg_assert_failed("unknown envelope type");
	}
}

void CEnvelope::OnAttach(CEditorMap *pMap)
{
	if(Map() != pMap)
	{
		m_Id = pMap->AllocateObjectId();
		for(auto &Element : m_vPoints)
			Element.m_Id = pMap->AllocateObjectId();
	}
	CMapObject::OnAttach(pMap);
}
