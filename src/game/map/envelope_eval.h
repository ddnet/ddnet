#ifndef GAME_MAP_ENVELOPE_EVAL_H
#define GAME_MAP_ENVELOPE_EVAL_H

#include <base/color.h>
#include <game/client/component.h>
#include <game/client/render.h>
#include <game/mapitems.h>

class CEditor;

class IEnvelopeEval
{
public:
	IEnvelopeEval() {}
	virtual ~IEnvelopeEval() = default;
	virtual void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly) = 0;
	virtual const IEnvelopePointAccess &PointAccess(int Env) const = 0;
	virtual std::shared_ptr<const CMapItemEnvelope> GetEnvelope(int Env) = 0;

private:
};

class CEnvelopeEvalGame : public IEnvelopeEval, public CComponentInterfaces
{
public:
	CEnvelopeEvalGame(CGameClient *pGameClient, IMap *pMap);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly) override;
	const IEnvelopePointAccess &PointAccess(int Env) const override { return m_EnvelopePointAccess; }
	std::shared_ptr<const CMapItemEnvelope> GetEnvelope(int Env) override;

private:
	IMap *m_pMap;
	CMapBasedEnvelopePointAccess m_EnvelopePointAccess;
};

class CEnvelopeEvalEditor : public IEnvelopeEval
{
public:
	CEnvelopeEvalEditor(CEditor *pEditor);
	void EnvelopeEval(int TimeOffsetMillis, int Env, ColorRGBA &Result, size_t Channels, bool OnlineOnly) override;
	const IEnvelopePointAccess &PointAccess(int Env) const override;
	std::shared_ptr<const CMapItemEnvelope> GetEnvelope(int Env) override;

private:
	CEditor *m_pEditor;
};

#endif
