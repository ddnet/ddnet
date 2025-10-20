#include "envelope.h"

#include <base/system.h>

#include <algorithm>
#include <chrono>
#include <limits>

using namespace std::chrono_literals;

CEnvelope::CEnvelopePointAccess::CEnvelopePointAccess(std::vector<CEnvPoint_runtime> *pvPoints)
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

CEnvelope::CEnvelope(EType Type) :
	m_Type(Type), m_PointsAccess(&m_vPoints) {}

CEnvelope::CEnvelope(int NumChannels) :
	m_PointsAccess(&m_vPoints)
{
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
		dbg_assert(false, "invalid number of channels for envelope");
	}
}

void CEnvelope::Resort()
{
	std::sort(m_vPoints.begin(), m_vPoints.end());
}

std::pair<float, float> CEnvelope::GetValueRange(int ChannelMask)
{
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
					Top = maximum(Top, v);
					Bottom = minimum(Bottom, v);
				}

				if(PointIndex < m_vPoints.size() - 1 && Point.m_Curvetype == CURVETYPE_BEZIER)
				{
					// out-tangent handle
					const float v = fx2f(Point.m_aValues[c] + Point.m_Bezier.m_aOutTangentDeltaY[c]);
					Top = maximum(Top, v);
					Bottom = minimum(Bottom, v);
				}

				if(PointIndex > 0 && m_vPoints[PointIndex - 1].m_Curvetype == CURVETYPE_BEZIER)
				{
					// in-tangent handle
					const float v = fx2f(Point.m_aValues[c] + Point.m_Bezier.m_aInTangentDeltaY[c]);
					Top = maximum(Top, v);
					Bottom = minimum(Bottom, v);
				}
			}
		}
	}
	return {Bottom, Top};
}

void CEnvelope::Eval(float Time, ColorRGBA &Result, size_t Channels)
{
	Channels = minimum<size_t>(Channels, GetChannels(), CEnvPoint::MAX_CHANNELS);
	CRenderMap::RenderEvalEnvelope(&m_PointsAccess, std::chrono::nanoseconds((int64_t)((double)Time * (double)std::chrono::nanoseconds(1s).count())), Result, Channels);
}

void CEnvelope::AddPoint(CFixedTime Time, std::array<int, CEnvPoint::MAX_CHANNELS> aValues)
{
	CEnvPoint_runtime Point;
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
