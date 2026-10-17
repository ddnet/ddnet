#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_H

#include "document_values.h"

#include <game/editor/map_object.h>
#include <game/map/render_map.h>
#include <game/mapitems.h>

#include <array>
#include <vector>

class CEnvelope : public CMapObject, public CEnvelopeValues
{
public:
	void OnAttach(CEditorMap *pMap) override;
	CEnvelope(CEditorMap *pMap, EType Type, std::uint64_t RetainedId = 0);
	CEnvelope(CEditorMap *pMap, int NumChannels);
	CEnvelope(const CEnvelope &) = delete;
	CEnvelope &operator=(const CEnvelope &) = delete;

	std::pair<float, float> GetValueRange(int ChannelMask);
	void Eval(float Time, ColorRGBA &Result, size_t Channels) const;
	void AddPoint(CFixedTime Time, std::array<int, CEnvPoint::MAX_CHANNELS> aValues);
	float EndTime() const;
	int FindPointIndex(CFixedTime Time) const;
	int GetChannels() const;
	EType Type() const { return m_Type; }

private:
	void Resort();

	class CEnvelopePointAccess : public IEnvelopePointAccess
	{
		const std::vector<CEnvelopePointValues> *m_pvPoints;

	public:
		CEnvelopePointAccess(const std::vector<CEnvelopePointValues> *pvPoints);

		int NumPoints() const override;
		const CEnvPoint *GetPoint(int Index) const override;
		const CEnvPointBezier *GetBezier(int Index) const override;
	};
	CEnvelopePointAccess m_PointsAccess;
};

#endif
