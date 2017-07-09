/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/gamecore.h>
#include <game/layers.h>
#include <game/animation.h>

void GetAnimationTransform(float GlobalTime, int Env, CLayers* pLayers, vec2& Position, float& Angle)
{
	CEnvPoint *pPoints = 0;
	
	Position.x = 0.0f;
	Position.y = 0.0f;
	Angle = 0.0f;
	
	{
		int Start, Num;
		pLayers->Map()->GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
		if(Num)
			pPoints = (CEnvPoint *)pLayers->Map()->GetItem(Start, 0, 0);
	}
	
	int Start, Num;
	pLayers->Map()->GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);

	if(Env >= Num)
		return;

	CMapItemEnvelope *pItem = (CMapItemEnvelope *)pLayers->Map()->GetItem(Start+Env, 0, 0);

	pPoints += pItem->m_StartPoint;

	if(pItem->m_NumPoints == 0)
		return;
	
	if(pItem->m_NumPoints == 1)
	{
		Position.x = fx2f(pPoints[0].m_aValues[0]);
		Position.y = fx2f(pPoints[0].m_aValues[1]);
		Angle = fx2f(pPoints[0].m_aValues[2])/360.0f*pi*2.0f;
		return;
	}

	float Time = fmod(GlobalTime, pPoints[pItem->m_NumPoints-1].m_Time/1000.0f)*1000.0f;
	for(int i = 0; i < pItem->m_NumPoints-1; i++)
	{
		if(Time >= pPoints[i].m_Time && Time <= pPoints[i+1].m_Time)
		{
			float Delta = pPoints[i+1].m_Time-pPoints[i].m_Time;
			float a = (Time-pPoints[i].m_Time)/Delta;


			if(pPoints[i].m_Curvetype == CURVETYPE_SMOOTH)
				a = -2*a*a*a + 3*a*a; // second hermite basis
			else if(pPoints[i].m_Curvetype == CURVETYPE_SLOW)
				a = a*a*a;
			else if(pPoints[i].m_Curvetype == CURVETYPE_FAST)
			{
				a = 1-a;
				a = 1-a*a*a;
			}
			else if (pPoints[i].m_Curvetype == CURVETYPE_STEP)
				a = 0;
			else
			{
				// linear
			}

			// X
			{
				float v0 = fx2f(pPoints[i].m_aValues[0]);
				float v1 = fx2f(pPoints[i+1].m_aValues[0]);
				Position.x = v0 + (v1-v0) * a;
			}
			// Y
			{
				float v0 = fx2f(pPoints[i].m_aValues[1]);
				float v1 = fx2f(pPoints[i+1].m_aValues[1]);
				Position.y = v0 + (v1-v0) * a;
			}
			// angle
			{
				float v0 = fx2f(pPoints[i].m_aValues[2]);
				float v1 = fx2f(pPoints[i+1].m_aValues[2]);
				Angle = (v0 + (v1-v0) * a)/360.0f*pi*2.0f;
			}

			return;
		}
	}

	Position.x = fx2f(pPoints[pItem->m_NumPoints-1].m_aValues[0]);
	Position.y = fx2f(pPoints[pItem->m_NumPoints-1].m_aValues[1]);
	Angle = fx2f(pPoints[pItem->m_NumPoints-1].m_aValues[2]);
	return;
}
