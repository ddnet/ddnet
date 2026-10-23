#include "image.h"

#include "map.h"

#include <base/mem.h>

#include <game/mapitems.h>

CEditorImage::CEditorImage(CEditorMap *pMap, std::uint64_t RetainedId) :
	CMapObject(pMap),
	m_Automapper(pMap)
{
	m_Id = RetainedId ? RetainedId : Map()->AllocateObjectId();
	m_Texture.Invalidate();
}

CEditorImage::~CEditorImage()
{
	Graphics()->UnloadTexture(&m_Texture);
}

void CEditorImage::OnAttach(CEditorMap *pMap)
{
	if(Map() != pMap)
		m_Id = pMap->AllocateObjectId();
	CMapObject::OnAttach(pMap);
	m_Automapper.OnAttach(pMap);
}

const std::array<unsigned char, 256> &CEditorImage::TileFlags() const
{
	const auto Content = m_Content ? m_Content->StorageIdentity() : std::weak_ptr<const void>{};
	if(!m_TileFlagsValid || Content.owner_before(m_TileFlagsContent) || m_TileFlagsContent.owner_before(Content) ||
		m_TileFlagsWidth != m_Width || m_TileFlagsHeight != m_Height || m_TileFlagsFormat != m_Format)
	{
		m_aTileFlags = EditorImageTileFlags(*this);
		m_TileFlagsContent = Content;
		m_TileFlagsWidth = m_Width;
		m_TileFlagsHeight = m_Height;
		m_TileFlagsFormat = m_Format;
		m_TileFlagsValid = true;
	}
	return m_aTileFlags;
}

void CEditorImage::Free()
{
	Graphics()->UnloadTexture(&m_Texture);
	m_Automapper.Unload();
	m_Content.reset();
	m_Width = 0;
	m_Height = 0;
	m_Format = CImageInfo::FORMAT_UNDEFINED;
}

CEditorImage &CEditorImage::operator=(CImageInfo &&Other)
{
	// Prepare owned immutable pixels before releasing existing runtime state.
	std::optional<editor_history::CResourceBlob> Content;
	if(Other.m_pData != nullptr)
		Content.emplace(std::span<const std::uint8_t>(Other.m_pData, Other.DataSize()));
	Graphics()->UnloadTexture(&m_Texture);
	m_Automapper.Unload();
	m_Width = Other.m_Width;
	m_Height = Other.m_Height;
	m_Format = Other.m_Format;
	m_Content = std::move(Content);
	Other.Free();
	return *this;
}

CImageInfo CEditorImage::ImageCopy() const
{
	CImageInfo Image;
	if(m_Content)
	{
		Image.m_Width = m_Width;
		Image.m_Height = m_Height;
		Image.m_Format = m_Format;
		Image.Allocate();
		if(!Image.m_pData)
			return {};
		mem_copy(Image.m_pData, Data(), DataSize());
	}
	return Image;
}

bool CEditorImage::DataEquals(const CEditorImage &Other) const
{
	return m_Width == Other.m_Width && m_Height == Other.m_Height && m_Format == Other.m_Format && m_Content == Other.m_Content;
}
