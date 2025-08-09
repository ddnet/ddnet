#ifndef GAME_EDITOR_REFERENCES_H
#define GAME_EDITOR_REFERENCES_H

#include <game/editor/mapitems/envelope.h>
#include <game/editor/mapitems/layer_quads.h>
#include <game/editor/mapitems/layer_sounds.h>
#include <game/editor/mapitems/layer_tiles.h>

class IEditorEnvelopeReference
{
public:
	virtual void SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex) = 0;
	virtual ~IEditorEnvelopeReference() = default;
};

class CLayerTilesEnvelopeReference : public IEditorEnvelopeReference
{
public:
	CLayerTilesEnvelopeReference(std::shared_ptr<CLayerTiles> pLayerTiles) :
		m_pLayerTiles(pLayerTiles) {}
	void SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex) override;

private:
	std::shared_ptr<CLayerTiles> m_pLayerTiles;
};

class CLayerQuadsEnvelopeReference : public IEditorEnvelopeReference
{
public:
	CLayerQuadsEnvelopeReference(std::shared_ptr<CLayerQuads> pLayerQuads) :
		m_pLayerQuads(pLayerQuads) {}
	void SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex) override;
	void AddQuadIndex(int QuadIndex) { m_vQuadIndices.push_back(QuadIndex); }
	bool Empty() const { return m_vQuadIndices.empty(); }

private:
	std::vector<int> m_vQuadIndices;
	std::shared_ptr<CLayerQuads> m_pLayerQuads;
};

class CLayerSoundEnvelopeReference : public IEditorEnvelopeReference
{
public:
	CLayerSoundEnvelopeReference(std::shared_ptr<CLayerSounds> pLayerSounds) :
		m_pLayerSounds(pLayerSounds) {}
	void SetEnvelope(const std::shared_ptr<CEnvelope> &pEnvelope, int EnvIndex) override;
	void AddSoundSourceIndex(int SoundSourceIndex) { m_vSoundSourceIndices.push_back(SoundSourceIndex); }
	bool Empty() const { return m_vSoundSourceIndices.empty(); }

private:
	std::vector<int> m_vSoundSourceIndices;
	std::shared_ptr<CLayerSounds> m_pLayerSounds;
};

#endif
