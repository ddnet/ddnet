#include "envelope.h"

#include <algorithm>
#include <chrono>
#include <limits>

using namespace std::chrono_literals;

CEnvelope::CEnvelopePointAccess::CEnvelopePointAccess(std::vector<CEnvelopePoint> *pvPoints)
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
	return &m_pvPoints->at(Index).m_PointInfo;
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
	for(size_t i = 0; i < m_vPoints.size(); i++)
	{
		for(int c = 0; c < GetChannels(); c++)
		{
			if(ChannelMask & (1 << c))
			{
				{
					// value handle
					const float v = m_vPoints[i].Value(c);
					Top = maximum(Top, v);
					Bottom = minimum(Bottom, v);
				}

				if(m_vPoints[i].CurveType() == CURVETYPE_BEZIER)
				{
					const float ValueIn = m_vPoints[i].InTangentHandle(c).y;
					Top = maximum(Top, ValueIn);
					Bottom = minimum(Bottom, ValueIn);
				}

				if(i > 0 && m_vPoints[i - 1].CurveType() == CURVETYPE_BEZIER)
				{
					const float ValueOut = m_vPoints[i].OutTangentHandle(c).y;
					Top = maximum(Top, ValueOut);
					Bottom = minimum(Bottom, ValueOut);
				}
			}
		}
	}

	return {Bottom, Top};
}

int CEnvelope::Eval(float Time, ColorRGBA &Color)
{
	CRenderTools::RenderEvalEnvelope(&m_PointsAccess, GetChannels(), std::chrono::nanoseconds((int64_t)((double)Time * (double)std::chrono::nanoseconds(1s).count())), Color);
	return GetChannels();
}

void CEnvelope::AddPoint(float Time, float Volume)
{
	CEnvelopePoint p(Time, CURVETYPE_LINEAR, {Volume});
	m_vPoints.emplace_back(p);
	Resort();
}

void CEnvelope::AddPoint(float Time, CTransform Transform)
{
	CEnvelopePoint p(Time, CURVETYPE_LINEAR, {Transform.OffsetX, Transform.OffsetY, Transform.Rotation});
	m_vPoints.emplace_back(p);
	Resort();
}

void CEnvelope::AddPoint(float Time, ColorRGBA Color)
{
	CEnvelopePoint p(Time, CURVETYPE_LINEAR, {Color.r, Color.g, Color.b, Color.a});
	m_vPoints.emplace_back(p);
	Resort();
}

void CEnvelope::RemovePoint(int Index)
{
	m_vPoints.erase(m_vPoints.begin() + Index);
}

float CEnvelope::EndTime() const
{
	if(m_vPoints.empty())
		return 0.0f;
	return m_vPoints.back().Time();
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
