#include "envelope_extrema.h"

CEnvelopeExtrema::CEnvelopeExtrema(IMap *pMap) :
	m_EnvelopePoints(pMap)
{
	m_pMap = pMap;

	m_EnvelopeExtremaItemNone.m_Available = true;
	m_EnvelopeExtremaItemNone.m_Rotating = false;
	m_EnvelopeExtremaItemNone.m_Minima = ivec2(0, 0);
	m_EnvelopeExtremaItemNone.m_Maxima = ivec2(0, 0);

	m_EnvelopeExtremaItemInvalid.m_Available = false;
	m_EnvelopeExtremaItemInvalid.m_Rotating = false;
	m_EnvelopeExtremaItemInvalid.m_Minima = ivec2(0, 0);
	m_EnvelopeExtremaItemInvalid.m_Maxima = ivec2(0, 0);

	CalculateExtrema();
}

void CEnvelopeExtrema::CalculateEnvelope(const CMapItemEnvelope *pEnvelopeItem, int EnvelopeIndex)
{
	CEnvelopeExtremaItem &EnvExt = m_vEnvelopeExtrema[EnvelopeIndex];

	// setup default values
	for(int Channel = 0; Channel < 2; ++Channel)
	{
		EnvExt.m_Minima[Channel] = std::numeric_limits<int>::max(); // minimum of channel
		EnvExt.m_Maxima[Channel] = std::numeric_limits<int>::min(); // maximum of channel
	}
	EnvExt.m_Available = false;
	EnvExt.m_Rotating = false;

	// check if the envelope is a valid position envelope
	if(!pEnvelopeItem || pEnvelopeItem->m_Channels != 3)
		return;

	for(int PointId = pEnvelopeItem->m_StartPoint; PointId < pEnvelopeItem->m_StartPoint + pEnvelopeItem->m_NumPoints; ++PointId)
	{
		const CEnvPoint *pEnvPoint = m_EnvelopePoints.GetPoint(PointId);
		if(!pEnvPoint)
			return;

		// check if quad is rotating
		if(pEnvPoint->m_aValues[2] != 0)
			EnvExt.m_Rotating = true;

		for(int Channel = 0; Channel < 2; ++Channel)
		{
			EnvExt.m_Minima[Channel] = std::min(pEnvPoint->m_aValues[Channel], EnvExt.m_Minima[Channel]);
			EnvExt.m_Maxima[Channel] = std::max(pEnvPoint->m_aValues[Channel], EnvExt.m_Maxima[Channel]);

			// bezier curves can have offsets beyond the fixed points
			// using the bezier position is just an estimate, but clipping like this is good enough
			if(PointId < pEnvelopeItem->m_StartPoint + pEnvelopeItem->m_NumPoints - 1 && pEnvPoint->m_Curvetype == CURVETYPE_BEZIER)
			{
				const CEnvPointBezier *pEnvPointBezier = m_EnvelopePoints.GetBezier(PointId);
				if(!pEnvPointBezier)
					return;

				// we are only interested in the height not in the time, meaning we only need delta Y
				EnvExt.m_Minima[Channel] = std::min(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aOutTangentDeltaY[Channel], EnvExt.m_Minima[Channel]);
				EnvExt.m_Maxima[Channel] = std::max(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aOutTangentDeltaY[Channel], EnvExt.m_Maxima[Channel]);
			}

			if(PointId > 0 && m_EnvelopePoints.GetPoint(PointId - 1)->m_Curvetype == CURVETYPE_BEZIER)
			{
				const CEnvPointBezier *pEnvPointBezier = m_EnvelopePoints.GetBezier(PointId);
				if(!pEnvPointBezier)
					return;

				// we are only interested in the height not in the time, meaning we only need delta Y
				EnvExt.m_Minima[Channel] = std::min(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aInTangentDeltaY[Channel], EnvExt.m_Minima[Channel]);
				EnvExt.m_Maxima[Channel] = std::max(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aInTangentDeltaY[Channel], EnvExt.m_Maxima[Channel]);
			}
		}
	}

	EnvExt.m_Available = true;
}

void CEnvelopeExtrema::CalculateExtrema()
{
	int EnvelopeStart, EnvelopeNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvelopeStart, &EnvelopeNum);
	m_vEnvelopeExtrema.resize(EnvelopeNum);
	for(int EnvelopeIndex = 0; EnvelopeIndex < EnvelopeNum; ++EnvelopeIndex)
	{
		const CMapItemEnvelope *pItem = static_cast<const CMapItemEnvelope *>(m_pMap->GetItem(EnvelopeStart + EnvelopeIndex));
		CalculateEnvelope(pItem, EnvelopeIndex);
	}
}

const CEnvelopeExtrema::CEnvelopeExtremaItem &CEnvelopeExtrema::GetExtrema(int EnvelopeIndex) const
{
	if(EnvelopeIndex == -1)
		return m_EnvelopeExtremaItemNone; // No envelope just means no movement
	else if(EnvelopeIndex < -1 || EnvelopeIndex >= (int)m_vEnvelopeExtrema.size())
		return m_EnvelopeExtremaItemInvalid;
	return m_vEnvelopeExtrema[EnvelopeIndex];
}
