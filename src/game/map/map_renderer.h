#ifndef GAME_MAP_MAP_RENDERER_H
#define GAME_MAP_MAP_RENDERER_H

#include <engine/map.h>

#include <game/layers.h>
#include <game/map/render_component.h>
#include <game/map/render_layer.h>

typedef std::function<void(int GroupId, const CRenderLayerParams &Params)> FGroupRenderCallback;

class CMapRenderer : public CRenderComponent
{
public:
	CMapRenderer() = default;

	void Clear();
	void Load(ERenderType Type, CLayers *pLayers, IMapImages *pMapImages, IEnvelopeEval *pEnvelopeEval, std::optional<FRenderUploadCallback> RenderCallbackOptional);
	void Render(const CRenderLayerParams &Params);

	void SetGroupRenderCallback(FGroupRenderCallback Callback) { m_GroupRenderCallback = std::move(Callback); }

private:
	int GetLayerType(const CMapItemLayer *pLayer, const CLayers *pLayers) const;

	std::vector<std::unique_ptr<CRenderLayer>> m_vpRenderLayers;
	FGroupRenderCallback m_GroupRenderCallback;
};

#endif
