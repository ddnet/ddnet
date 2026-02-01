#ifndef GAME_EDITOR_MAPITEMS_ENVELOPE_H
#define GAME_EDITOR_MAPITEMS_ENVELOPE_H

#include <game/map/render_map.h>
#include <game/mapitems.h>

#include <array>
#include <vector>

class CEnvelope
{
public:
	std::vector<CEnvPoint_runtime> m_vPoints;
	char m_aName[32] = "";
	bool m_Synchronized = true;

	enum class EType
	{
		POSITION,
		COLOR,
		SOUND,
	};
	explicit CEnvelope(EType Type);
	explicit CEnvelope(int NumChannels);

	std::pair<float, float> GetValueRange(int ChannelMask);
	void Eval(float Time, ColorRGBA &Result, size_t Channels);
	void AddPoint(CFixedTime Time, std::array<int, CEnvPoint::MAX_CHANNELS> aValues);
	float EndTime() const;
	int FindPointIndex(CFixedTime Time) const;
	int GetChannels() const;
	EType Type() const { return m_Type; }

private:
	void Resort();

	EType m_Type;

	class CEnvelopePointAccess : public IEnvelopePointAccess
	{
		std::vector<CEnvPoint_runtime> *m_pvPoints;

	public:
		CEnvelopePointAccess(std::vector<CEnvPoint_runtime> *pvPoints);

		int NumPoints() const override;
		const CEnvPoint *GetPoint(int Index) const override;
		const CEnvPointBezier *GetBezier(int Index) const override;
	};
	CEnvelopePointAccess m_PointsAccess;
};

#endif
