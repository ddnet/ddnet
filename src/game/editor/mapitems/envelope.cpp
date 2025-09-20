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

CEnvelope::CEnvelope(const CEnvelope &Other) :
	m_vPoints(Other.m_vPoints), m_Synchronized(Other.m_Synchronized), m_Type(Other.m_Type), m_PointsAccess(&m_vPoints)
{
	str_copy(m_aName, Other.m_aName);
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

void CEnvelope::AddPoint(CFixedTime Time, int v0, int v1, int v2, int v3)
{
	CEnvPoint_runtime p;
	p.m_Time = Time;
	p.m_aValues[0] = v0;
	p.m_aValues[1] = v1;
	p.m_aValues[2] = v2;
	p.m_aValues[3] = v3;
	p.m_Curvetype = CURVETYPE_LINEAR;
	for(int c = 0; c < CEnvPoint::MAX_CHANNELS; c++)
	{
		p.m_Bezier.m_aInTangentDeltaX[c] = CFixedTime(0);
		p.m_Bezier.m_aInTangentDeltaY[c] = 0;
		p.m_Bezier.m_aOutTangentDeltaX[c] = CFixedTime(0);
		p.m_Bezier.m_aOutTangentDeltaY[c] = 0;
	}
	m_vPoints.push_back(p);
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
		dbg_assert(false, "unknown envelope type");
		dbg_break();
	}
}
