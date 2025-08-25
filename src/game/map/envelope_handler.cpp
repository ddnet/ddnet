#include "envelope_handler.h"

CEnvelopeHandler::CEnvelopeHandler(IEnvelopeEval *pEnvelopeEval, IMap *pMap) :
	m_EnvelopePoints(pMap)
{
	m_pEnvelopeEval = pEnvelopeEval;
	m_pMap = pMap;
	m_EnvelopeExtramaNone.m_Available = true;
	m_EnvelopeExtramaNone.m_Minima = ivec2(0, 0);
	m_EnvelopeExtramaNone.m_Maxima = ivec2(0, 0);

	CalculateExtrema();
}

void CEnvelopeHandler::CalculateEnvelope(const CMapItemEnvelope *pEnvelopeItem, int EnvId)
{
	CEnvelopeExtrema &EnvExt = m_vEnvelopeExtrema[EnvId];

	// setup default values
	for(int Channel = 0; Channel < 2; ++Channel)
	{
		EnvExt.m_Minima[Channel] = std::numeric_limits<int>::max(); // minimum of channel
		EnvExt.m_Maxima[Channel] = std::numeric_limits<int>::min(); // maximum of channel
	}
	EnvExt.m_Available = false;

	// check if the envelope is a position envelope
	if(pEnvelopeItem->m_Channels != 3)
		return;

	for(int PointId = pEnvelopeItem->m_StartPoint; PointId < pEnvelopeItem->m_StartPoint + pEnvelopeItem->m_NumPoints; ++PointId)
	{
		const CEnvPoint *pEnvPoint = m_EnvelopePoints.GetPoint(PointId);

		// rotation is not implemented for clipping
		if(pEnvPoint->m_aValues[2] != 0)
			return;

		for(int Channel = 0; Channel < 2; ++Channel)
		{
			EnvExt.m_Minima[Channel] = std::min(pEnvPoint->m_aValues[Channel], EnvExt.m_Minima[Channel]);
			EnvExt.m_Maxima[Channel] = std::max(pEnvPoint->m_aValues[Channel], EnvExt.m_Maxima[Channel]);

			// bezier curves can have offsets beyond the fixed points
			// using the bezier position is just an estimate, but clipping like this is good enough
			if(PointId < pEnvelopeItem->m_StartPoint + pEnvelopeItem->m_NumPoints - 1 && pEnvPoint->m_Curvetype == CURVETYPE_BEZIER)
			{
				const CEnvPointBezier *pEnvPointBezier = m_EnvelopePoints.GetBezier(PointId);
				// we are only interested in the height not in the time, meaning we only need delta Y
				EnvExt.m_Minima[Channel] = std::min(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aOutTangentDeltaY[Channel], EnvExt.m_Minima[Channel]);
				EnvExt.m_Maxima[Channel] = std::max(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aOutTangentDeltaY[Channel], EnvExt.m_Maxima[Channel]);
			}

			if(PointId > 0 && m_EnvelopePoints.GetPoint(PointId - 1)->m_Curvetype == CURVETYPE_BEZIER)
			{
				const CEnvPointBezier *pEnvPointBezier = m_EnvelopePoints.GetBezier(PointId);
				// we are only interested in the height not in the time, meaning we only need delta Y
				EnvExt.m_Minima[Channel] = std::min(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aInTangentDeltaY[Channel], EnvExt.m_Minima[Channel]);
				EnvExt.m_Maxima[Channel] = std::max(pEnvPoint->m_aValues[Channel] + pEnvPointBezier->m_aInTangentDeltaY[Channel], EnvExt.m_Maxima[Channel]);
			}
		}
	}

	EnvExt.m_Available = true;
}

void CEnvelopeHandler::CalculateExtrema()
{
	int EnvStart, EnvNum;
	m_pMap->GetType(MAPITEMTYPE_ENVELOPE, &EnvStart, &EnvNum);
	m_vEnvelopeExtrema.resize(EnvNum);
	for(int EnvId = 0; EnvId < EnvNum; ++EnvId)
	{
		const CMapItemEnvelope *pItem = static_cast<const CMapItemEnvelope *>(m_pMap->GetItem(EnvStart + EnvId));
		CalculateEnvelope(pItem, EnvId);
	}
}

const CEnvelopeHandler::CEnvelopeExtrema &CEnvelopeHandler::GetExtrema(int Env) const
{
	if(Env == -1)
		return m_EnvelopeExtramaNone;
	return m_vEnvelopeExtrema[Env];
}
