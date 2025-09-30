#ifndef GAME_SERVER_FOXNET_ENVELOPEACCESS_H
#define GAME_SERVER_FOXNET_ENVELOPEACCESS_H

#include <base/vmath.h>

class CFixedTime;
class CEnvPoint;
class CEnvPointBezier;
class CEnvPointBezier_upstream;

class IEnvelopePointAccess
{
public:
	virtual ~IEnvelopePointAccess() = default;
	virtual int NumPoints() const = 0;
	virtual const CEnvPoint *GetPoint(int Index) const = 0;
	virtual const CEnvPointBezier *GetBezier(int Index) const = 0;
	int FindPointIndex(CFixedTime Time) const;
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

float SolveBezier(float x, float p0, float p1, float p2, float p3);
#endif // GAME_SERVER_FOXNET_ENVELOPEACCESS_H