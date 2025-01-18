/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_ENVELOPE_ACCESS_H
#define GAME_ENVELOPE_ACCESS_H

#include <base/vmath.h>

struct CEnvPoint;
struct CEnvPointBezier;
struct CEnvPointBezier_upstream;

class IEnvelopePointAccess
{
public:
	virtual ~IEnvelopePointAccess() = default;
	virtual int NumPoints() const = 0;
	virtual const CEnvPoint *GetPoint(int Index) const = 0;
	virtual const CEnvPointBezier *GetBezier(int Index) const = 0;
};

class CMapBasedEnvelopePointAccess : public IEnvelopePointAccess
{
	int m_StartPoint;
	int m_NumPoints;
	int m_NumPointsMax;
	CEnvPoint *m_pPoints;
	CEnvPointBezier *m_pPointsBezier;
	CEnvPointBezier_upstream *m_pPointsBezierUpstream;

public:
	CMapBasedEnvelopePointAccess(class CDataFileReader *pReader);
	CMapBasedEnvelopePointAccess(class IMap *pMap);
	void SetPointsRange(int StartPoint, int NumPoints);
	int StartPoint() const;
	int NumPoints() const override;
	int NumPointsMax() const;
	const CEnvPoint *GetPoint(int Index) const override;
	const CEnvPointBezier *GetBezier(int Index) const override;
};

//TODO: Those functions should not be here
float SolveBezier(float x, float p0, float p1, float p2, float p3);

#endif
