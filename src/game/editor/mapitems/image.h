#ifndef GAME_EDITOR_MAPITEMS_IMAGE_H
#define GAME_EDITOR_MAPITEMS_IMAGE_H

#include "document_values.h"

#include <base/types.h>

#include <engine/graphics.h>

#include <game/editor/auto_map.h>
#include <game/editor/map_object.h>

class CEditorImage : public CMapObject, public CEditorImageValues
{
	mutable std::weak_ptr<const void> m_TileFlagsContent;
	mutable std::size_t m_TileFlagsWidth = 0;
	mutable std::size_t m_TileFlagsHeight = 0;
	mutable CImageInfo::EImageFormat m_TileFlagsFormat = CImageInfo::FORMAT_UNDEFINED;
	mutable std::array<unsigned char, 256> m_aTileFlags{};
	mutable bool m_TileFlagsValid = false;

public:
	explicit CEditorImage(CEditorMap *pMap, std::uint64_t RetainedId = 0);
	CEditorImage(const CEditorImage &) = delete;
	CEditorImage &operator=(const CEditorImage &) = delete;
	~CEditorImage() override;
	void OnAttach(CEditorMap *pMap) override;

	/** Derived opacity follows immutable pixel ownership and image geometry. */
	const std::array<unsigned char, 256> &TileFlags() const;
	void Free();

	CEditorImage &operator=(CImageInfo &&Other);
	CImageInfo ImageCopy() const;
	bool DataEquals(const CEditorImage &Other) const;

	IGraphics::CTextureHandle m_Texture;

	CAutomapper m_Automapper;
};

#endif
