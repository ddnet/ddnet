/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>
#include <base/math.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/json.h>
#include <engine/storage.h>
#include <engine/textrender.h>

// ft2 texture
#include <ft2build.h>
#include FT_FREETYPE_H

#include <chrono>
#include <cstddef>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace std::chrono_literals;

enum
{
	FONT_NAME_SIZE = 128,
};

struct SGlyph
{
	enum class EState
	{
		UNINITIALIZED,
		RENDERED,
		ERROR,
	};
	EState m_State = EState::UNINITIALIZED;

	int m_FontSize;
	FT_Face m_Face;
	int m_Chr;
	FT_UInt m_GlyphIndex;

	// these values are scaled to the font size
	// width * font_size == real_size
	float m_Width;
	float m_Height;
	float m_CharWidth;
	float m_CharHeight;
	float m_OffsetX;
	float m_OffsetY;
	float m_AdvanceX;

	float m_aUVs[4];
};

struct SGlyphKeyHash
{
	size_t operator()(const std::tuple<FT_Face, int, int> &Key) const
	{
		size_t Hash = 17;
		Hash = Hash * 31 + std::hash<FT_Face>()(std::get<0>(Key));
		Hash = Hash * 31 + std::hash<int>()(std::get<1>(Key));
		Hash = Hash * 31 + std::hash<int>()(std::get<2>(Key));
		return Hash;
	}
};

struct SGlyphKeyEquals
{
	bool operator()(const std::tuple<FT_Face, int, int> &Lhs, const std::tuple<FT_Face, int, int> &Rhs) const
	{
		return std::get<0>(Lhs) == std::get<0>(Rhs) && std::get<1>(Lhs) == std::get<1>(Rhs) && std::get<2>(Lhs) == std::get<2>(Rhs);
	}
};

class CAtlas
{
	struct SSectionKeyHash
	{
		size_t operator()(const std::tuple<size_t, size_t> &Key) const
		{
			// Width and height should never be above 2^16 so this hash should cause no collisions
			return (std::get<0>(Key) << 16) ^ std::get<1>(Key);
		}
	};

	struct SSectionKeyEquals
	{
		bool operator()(const std::tuple<size_t, size_t> &Lhs, const std::tuple<size_t, size_t> &Rhs) const
		{
			return std::get<0>(Lhs) == std::get<0>(Rhs) && std::get<1>(Lhs) == std::get<1>(Rhs);
		}
	};

	struct SSection
	{
		size_t m_X;
		size_t m_Y;
		size_t m_W;
		size_t m_H;

		SSection() = default;

		SSection(size_t X, size_t Y, size_t W, size_t H) :
			m_X(X), m_Y(Y), m_W(W), m_H(H)
		{
		}
	};

	/**
	 * Sections with a smaller width or height will not be created
	 * when cutting larger sections, to prevent collecting many
	 * small, mostly unusable sections.
	 */
	static constexpr size_t MIN_SECTION_DIMENSION = 6;

	/**
	 * Sections with larger width or height will be stored in m_vSections.
	 * Sections with width and height equal or smaller will be stored in m_SectionsMap.
	 * This achieves a good balance between the size of the vector storing all large
	 * sections and the map storing vectors of all sections with specific small sizes.
	 * Lowering this value will result in the size of m_vSections becoming the bottleneck.
	 * Increasing this value will result in the map becoming the bottleneck.
	 */
	static constexpr size_t MAX_SECTION_DIMENSION_MAPPED = 8 * MIN_SECTION_DIMENSION;

	size_t m_TextureDimension;
	std::vector<SSection> m_vSections;
	std::unordered_map<std::tuple<size_t, size_t>, std::vector<SSection>, SSectionKeyHash, SSectionKeyEquals> m_SectionsMap;

	void AddSection(size_t X, size_t Y, size_t W, size_t H)
	{
		std::vector<SSection> &vSections = W <= MAX_SECTION_DIMENSION_MAPPED && H <= MAX_SECTION_DIMENSION_MAPPED ? m_SectionsMap[std::make_tuple(W, H)] : m_vSections;
		vSections.emplace_back(X, Y, W, H);
	}

	void UseSection(const SSection &Section, size_t Width, size_t Height, int &PosX, int &PosY)
	{
		PosX = Section.m_X;
		PosY = Section.m_Y;

		// Create cut sections
		const size_t CutW = Section.m_W - Width;
		const size_t CutH = Section.m_H - Height;
		if(CutW == 0)
		{
			if(CutH >= MIN_SECTION_DIMENSION)
				AddSection(Section.m_X, Section.m_Y + Height, Section.m_W, CutH);
		}
		else if(CutH == 0)
		{
			if(CutW >= MIN_SECTION_DIMENSION)
				AddSection(Section.m_X + Width, Section.m_Y, CutW, Section.m_H);
		}
		else if(CutW > CutH)
		{
			if(CutW >= MIN_SECTION_DIMENSION)
				AddSection(Section.m_X + Width, Section.m_Y, CutW, Section.m_H);
			if(CutH >= MIN_SECTION_DIMENSION)
				AddSection(Section.m_X, Section.m_Y + Height, Width, CutH);
		}
		else
		{
			if(CutH >= MIN_SECTION_DIMENSION)
				AddSection(Section.m_X, Section.m_Y + Height, Section.m_W, CutH);
			if(CutW >= MIN_SECTION_DIMENSION)
				AddSection(Section.m_X + Width, Section.m_Y, CutW, Height);
		}
	}

public:
	void Clear(size_t TextureDimension)
	{
		m_TextureDimension = TextureDimension;
		m_vSections.clear();
		m_vSections.emplace_back(0, 0, m_TextureDimension, m_TextureDimension);
		m_SectionsMap.clear();
	}

	void IncreaseDimension(size_t NewTextureDimension)
	{
		dbg_assert(NewTextureDimension == m_TextureDimension * 2, "New atlas dimension must be twice the old one");
		// Create 3 square sections to cover the new area, add the sections
		// to the beginning of the vector so they are considered last.
		m_vSections.emplace_back(m_TextureDimension, m_TextureDimension, m_TextureDimension, m_TextureDimension);
		m_vSections.emplace_back(m_TextureDimension, 0, m_TextureDimension, m_TextureDimension);
		m_vSections.emplace_back(0, m_TextureDimension, m_TextureDimension, m_TextureDimension);
		std::rotate(m_vSections.rbegin(), m_vSections.rbegin() + 3, m_vSections.rend());
		m_TextureDimension = NewTextureDimension;
	}

	bool Add(size_t Width, size_t Height, int &PosX, int &PosY)
	{
		if(m_vSections.empty() || m_TextureDimension < Width || m_TextureDimension < Height)
			return false;

		// Find small section more efficiently by using maps
		if(Width <= MAX_SECTION_DIMENSION_MAPPED && Height <= MAX_SECTION_DIMENSION_MAPPED)
		{
			const auto UseSectionFromVector = [&](std::vector<SSection> &vSections) {
				if(!vSections.empty())
				{
					const SSection Section = vSections.back();
					vSections.pop_back();
					UseSection(Section, Width, Height, PosX, PosY);
					return true;
				}
				return false;
			};

			if(UseSectionFromVector(m_SectionsMap[std::make_tuple(Width, Height)]))
				return true;

			for(size_t CheckWidth = Width + 1; CheckWidth <= MAX_SECTION_DIMENSION_MAPPED; ++CheckWidth)
			{
				if(UseSectionFromVector(m_SectionsMap[std::make_tuple(CheckWidth, Height)]))
					return true;
			}

			for(size_t CheckHeight = Height + 1; CheckHeight <= MAX_SECTION_DIMENSION_MAPPED; ++CheckHeight)
			{
				if(UseSectionFromVector(m_SectionsMap[std::make_tuple(Width, CheckHeight)]))
					return true;
			}

			// We don't iterate sections in the map with increasing width and height at the same time,
			// because it's slower and doesn't noticeable increase the atlas utilization.
		}

		// Check vector for larger section
		size_t SmallestLossValue = std::numeric_limits<size_t>::max();
		size_t SmallestLossIndex = m_vSections.size();
		size_t SectionIndex = m_vSections.size();
		do
		{
			--SectionIndex;
			const SSection &Section = m_vSections[SectionIndex];
			if(Section.m_W < Width || Section.m_H < Height)
				continue;

			const size_t LossW = Section.m_W - Width;
			const size_t LossH = Section.m_H - Height;

			size_t Loss;
			if(LossW == 0)
				Loss = LossH;
			else if(LossH == 0)
				Loss = LossW;
			else
				Loss = LossW * LossH;

			if(Loss < SmallestLossValue)
			{
				SmallestLossValue = Loss;
				SmallestLossIndex = SectionIndex;
				if(SmallestLossValue == 0)
					break;
			}
		} while(SectionIndex > 0);
		if(SmallestLossIndex == m_vSections.size())
			return false; // No usable section found in vector

		// Use the section with the smallest loss
		const SSection Section = m_vSections[SmallestLossIndex];
		m_vSections.erase(m_vSections.begin() + SmallestLossIndex);
		UseSection(Section, Width, Height, PosX, PosY);
		return true;
	}
};

class CGlyphMap
{
public:
	enum
	{
		FONT_TEXTURE_FILL = 0, // the main text body
		FONT_TEXTURE_OUTLINE, // the text outline
		NUM_FONT_TEXTURES,
	};

private:
	/**
	 * The initial dimension of the atlas textures.
	 * Results in 1 MB of memory being used per texture.
	 */
	static constexpr int INITIAL_ATLAS_DIMENSION = 1024;

	/**
	 * The maximum dimension of the atlas textures.
	 * Results in 256 MB of memory being used per texture.
	 */
	static constexpr int MAXIMUM_ATLAS_DIMENSION = 16 * 1024;

	/**
	 * The minimum supported font size.
	 */
	static constexpr int MIN_FONT_SIZE = 6;

	/**
	 * The maximum supported font size.
	 */
	static constexpr int MAX_FONT_SIZE = 128;

	/**
	 * White square to indicate missing glyph.
	 */
	static constexpr int REPLACEMENT_CHARACTER = 0x25a1;

	IGraphics *m_pGraphics;
	IGraphics *Graphics() { return m_pGraphics; }

	// Atlas textures and data
	IGraphics::CTextureHandle m_aTextures[NUM_FONT_TEXTURES];
	// Width and height are the same, all font textures have the same dimensions
	size_t m_TextureDimension = INITIAL_ATLAS_DIMENSION;
	// Keep the full texture data, because OpenGL doesn't provide texture copying
	uint8_t *m_apTextureData[NUM_FONT_TEXTURES];
	CAtlas m_TextureAtlas;
	std::unordered_map<std::tuple<FT_Face, int, int>, SGlyph, SGlyphKeyHash, SGlyphKeyEquals> m_Glyphs;

	// Font faces
	FT_Face m_DefaultFace = nullptr;
	FT_Face m_IconFace = nullptr;
	FT_Face m_VariantFace = nullptr;
	FT_Face m_SelectedFace = nullptr;
	std::vector<FT_Face> m_vFallbackFaces;
	std::vector<FT_Face> m_vFtFaces;

	FT_Face GetFaceByName(const char *pFamilyName)
	{
		if(pFamilyName == nullptr || pFamilyName[0] == '\0')
			return nullptr;

		FT_Face FamilyNameMatch = nullptr;
		char aFamilyStyleName[FONT_NAME_SIZE];

		for(const auto &CurrentFace : m_vFtFaces)
		{
			// Best match: font face with matching family and style name
			str_format(aFamilyStyleName, sizeof(aFamilyStyleName), "%s %s", CurrentFace->family_name, CurrentFace->style_name);
			if(str_comp(pFamilyName, aFamilyStyleName) == 0)
			{
				return CurrentFace;
			}

			// Second best match: font face with matching family
			if(!FamilyNameMatch && str_comp(pFamilyName, CurrentFace->family_name) == 0)
			{
				FamilyNameMatch = CurrentFace;
			}
		}

		return FamilyNameMatch;
	}

	bool IncreaseGlyphMapSize()
	{
		if(m_TextureDimension >= MAXIMUM_ATLAS_DIMENSION)
			return false;

		const size_t NewTextureDimension = m_TextureDimension * 2;
		log_debug("textrender", "Increasing atlas dimension to %" PRIzu " (%" PRIzu " MB used for textures)", NewTextureDimension, (NewTextureDimension / 1024) * (NewTextureDimension / 1024) * NUM_FONT_TEXTURES);
		UnloadTextures();

		for(auto &pTextureData : m_apTextureData)
		{
			uint8_t *pTmpTexBuffer = new uint8_t[NewTextureDimension * NewTextureDimension];
			mem_zero(pTmpTexBuffer, NewTextureDimension * NewTextureDimension * sizeof(uint8_t));
			for(size_t y = 0; y < m_TextureDimension; ++y)
			{
				mem_copy(&pTmpTexBuffer[y * NewTextureDimension], &pTextureData[y * m_TextureDimension], m_TextureDimension);
			}
			delete[] pTextureData;
			pTextureData = pTmpTexBuffer;
		}

		m_TextureAtlas.IncreaseDimension(NewTextureDimension);

		m_TextureDimension = NewTextureDimension;

		UploadTextures();
		return true;
	}

	void UploadTextures()
	{
		const size_t NewTextureSize = m_TextureDimension * m_TextureDimension;
		uint8_t *pTmpTextFillData = static_cast<uint8_t *>(malloc(NewTextureSize));
		uint8_t *pTmpTextOutlineData = static_cast<uint8_t *>(malloc(NewTextureSize));
		mem_copy(pTmpTextFillData, m_apTextureData[FONT_TEXTURE_FILL], NewTextureSize);
		mem_copy(pTmpTextOutlineData, m_apTextureData[FONT_TEXTURE_OUTLINE], NewTextureSize);
		Graphics()->LoadTextTextures(m_TextureDimension, m_TextureDimension, m_aTextures[FONT_TEXTURE_FILL], m_aTextures[FONT_TEXTURE_OUTLINE], pTmpTextFillData, pTmpTextOutlineData);
	}

	void UnloadTextures()
	{
		Graphics()->UnloadTextTextures(m_aTextures[FONT_TEXTURE_FILL], m_aTextures[FONT_TEXTURE_OUTLINE]);
	}

	FT_UInt GetCharGlyph(int Chr, FT_Face *pFace, bool AllowReplacementCharacter)
	{
		for(FT_Face Face : {m_SelectedFace, m_DefaultFace, m_VariantFace})
		{
			if(Face && Face->charmap)
			{
				FT_UInt GlyphIndex = FT_Get_Char_Index(Face, (FT_ULong)Chr);
				if(GlyphIndex)
				{
					*pFace = Face;
					return GlyphIndex;
				}
			}
		}

		for(const auto &FallbackFace : m_vFallbackFaces)
		{
			if(FallbackFace->charmap)
			{
				FT_UInt GlyphIndex = FT_Get_Char_Index(FallbackFace, (FT_ULong)Chr);
				if(GlyphIndex)
				{
					*pFace = FallbackFace;
					return GlyphIndex;
				}
			}
		}

		if(!m_DefaultFace || !m_DefaultFace->charmap || !AllowReplacementCharacter)
		{
			*pFace = nullptr;
			return 0;
		}

		FT_UInt GlyphIndex = FT_Get_Char_Index(m_DefaultFace, (FT_ULong)REPLACEMENT_CHARACTER);
		*pFace = m_DefaultFace;

		if(GlyphIndex == 0)
		{
			log_debug("textrender", "Default font has no glyph for either %d or replacement char %d.", Chr, REPLACEMENT_CHARACTER);
		}

		return GlyphIndex;
	}

	void Grow(const unsigned char *pIn, unsigned char *pOut, int w, int h, int OutlineCount) const
	{
		for(int y = 0; y < h; y++)
		{
			for(int x = 0; x < w; x++)
			{
				int c = pIn[y * w + x];

				for(int sy = -OutlineCount; sy <= OutlineCount; sy++)
				{
					for(int sx = -OutlineCount; sx <= OutlineCount; sx++)
					{
						int GetX = x + sx;
						int GetY = y + sy;
						if(GetX >= 0 && GetY >= 0 && GetX < w && GetY < h)
						{
							int Index = GetY * w + GetX;
							float Mask = 1.f - clamp(length(vec2(sx, sy)) - OutlineCount, 0.f, 1.f);
							c = maximum(c, int(pIn[Index] * Mask));
						}
					}
				}

				pOut[y * w + x] = c;
			}
		}
	}

	int AdjustOutlineThicknessToFontSize(int OutlineThickness, int FontSize) const
	{
		if(FontSize > 48)
			OutlineThickness *= 4;
		else if(FontSize >= 18)
			OutlineThickness *= 2;
		return OutlineThickness;
	}

	void UploadGlyph(int TextureIndex, int PosX, int PosY, size_t Width, size_t Height, uint8_t *pData)
	{
		for(size_t y = 0; y < Height; ++y)
		{
			mem_copy(&m_apTextureData[TextureIndex][PosX + ((y + PosY) * m_TextureDimension)], &pData[y * Width], Width);
		}
		Graphics()->UpdateTextTexture(m_aTextures[TextureIndex], PosX, PosY, Width, Height, pData, true);
	}

	bool FitGlyph(size_t Width, size_t Height, int &PosX, int &PosY)
	{
		return m_TextureAtlas.Add(Width, Height, PosX, PosY);
	}

	bool RenderGlyph(SGlyph &Glyph)
	{
		FT_Set_Pixel_Sizes(Glyph.m_Face, 0, Glyph.m_FontSize);

		if(FT_Load_Glyph(Glyph.m_Face, Glyph.m_GlyphIndex, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP))
		{
			log_debug("textrender", "Error loading glyph. Chr=%d GlyphIndex=%u", Glyph.m_Chr, Glyph.m_GlyphIndex);
			return false;
		}

		const FT_Bitmap *pBitmap = &Glyph.m_Face->glyph->bitmap;
		if(pBitmap->pixel_mode != FT_PIXEL_MODE_GRAY)
		{
			log_debug("textrender", "Error loading glyph, unsupported pixel mode. Chr=%d GlyphIndex=%u PixelMode=%d", Glyph.m_Chr, Glyph.m_GlyphIndex, pBitmap->pixel_mode);
			return false;
		}

		const unsigned RealWidth = pBitmap->width;
		const unsigned RealHeight = pBitmap->rows;

		// adjust spacing
		int OutlineThickness = 0;
		int x = 0;
		int y = 0;
		if(RealWidth > 0)
		{
			OutlineThickness = AdjustOutlineThicknessToFontSize(1, Glyph.m_FontSize);
			x += (OutlineThickness + 1);
			y += (OutlineThickness + 1);
		}

		const unsigned Width = RealWidth + x * 2;
		const unsigned Height = RealHeight + y * 2;

		int X = 0;
		int Y = 0;

		if(Width > 0 && Height > 0)
		{
			// find space in atlas, or increase size if necessary
			while(!FitGlyph(Width, Height, X, Y))
			{
				if(!IncreaseGlyphMapSize())
				{
					log_debug("textrender", "Cannot fit glyph into atlas, which is already at maximum size. Chr=%d GlyphIndex=%u", Glyph.m_Chr, Glyph.m_GlyphIndex);
					return false;
				}
			}

			// prepare glyph data
			const size_t GlyphDataSize = (size_t)Width * Height * sizeof(uint8_t);
			uint8_t *pGlyphDataFill = static_cast<uint8_t *>(malloc(GlyphDataSize));
			uint8_t *pGlyphDataOutline = static_cast<uint8_t *>(malloc(GlyphDataSize));
			mem_zero(pGlyphDataFill, GlyphDataSize);
			for(unsigned py = 0; py < pBitmap->rows; ++py)
			{
				mem_copy(&pGlyphDataFill[(py + y) * Width + x], &pBitmap->buffer[py * pBitmap->width], pBitmap->width);
			}
			Grow(pGlyphDataFill, pGlyphDataOutline, Width, Height, OutlineThickness);

			// upload the glyph
			UploadGlyph(FONT_TEXTURE_FILL, X, Y, Width, Height, pGlyphDataFill);
			UploadGlyph(FONT_TEXTURE_OUTLINE, X, Y, Width, Height, pGlyphDataOutline);
		}

		// set glyph info
		{
			Glyph.m_Height = Height;
			Glyph.m_Width = Width;
			Glyph.m_CharHeight = RealHeight;
			Glyph.m_CharWidth = RealWidth;
			Glyph.m_OffsetX = (Glyph.m_Face->glyph->metrics.horiBearingX >> 6);
			Glyph.m_OffsetY = -((Glyph.m_Face->glyph->metrics.height >> 6) - (Glyph.m_Face->glyph->metrics.horiBearingY >> 6));
			Glyph.m_AdvanceX = (Glyph.m_Face->glyph->advance.x >> 6);

			Glyph.m_aUVs[0] = X;
			Glyph.m_aUVs[1] = Y;
			Glyph.m_aUVs[2] = Glyph.m_aUVs[0] + Width;
			Glyph.m_aUVs[3] = Glyph.m_aUVs[1] + Height;

			Glyph.m_State = SGlyph::EState::RENDERED;
		}
		return true;
	}

public:
	CGlyphMap(IGraphics *pGraphics)
	{
		m_pGraphics = pGraphics;
		for(auto &pTextureData : m_apTextureData)
		{
			pTextureData = new uint8_t[m_TextureDimension * m_TextureDimension];
			mem_zero(pTextureData, m_TextureDimension * m_TextureDimension * sizeof(uint8_t));
		}

		m_TextureAtlas.Clear(m_TextureDimension);
		UploadTextures();
	}

	~CGlyphMap()
	{
		UnloadTextures();
		for(auto &pTextureData : m_apTextureData)
		{
			delete[] pTextureData;
		}
	}

	FT_Face DefaultFace() const
	{
		return m_DefaultFace;
	}

	FT_Face IconFace() const
	{
		return m_IconFace;
	}

	void AddFace(FT_Face Face)
	{
		m_vFtFaces.push_back(Face);
	}

	bool SetDefaultFaceByName(const char *pFamilyName)
	{
		m_DefaultFace = GetFaceByName(pFamilyName);
		if(!m_DefaultFace)
		{
			if(!m_vFtFaces.empty())
			{
				m_DefaultFace = m_vFtFaces.front();
			}
			log_error("textrender", "The default font face '%s' could not be found", pFamilyName);
			return false;
		}
		return true;
	}

	bool SetIconFaceByName(const char *pFamilyName)
	{
		m_IconFace = GetFaceByName(pFamilyName);
		if(!m_IconFace)
		{
			log_error("textrender", "The icon font face '%s' could not be found", pFamilyName);
			return false;
		}
		return true;
	}

	bool AddFallbackFaceByName(const char *pFamilyName)
	{
		FT_Face Face = GetFaceByName(pFamilyName);
		if(!Face)
		{
			log_error("textrender", "The fallback font face '%s' could not be found", pFamilyName);
			return false;
		}
		if(std::find(m_vFallbackFaces.begin(), m_vFallbackFaces.end(), Face) != m_vFallbackFaces.end())
		{
			log_warn("textrender", "The fallback font face '%s' was specified multiple times", pFamilyName);
			return true;
		}
		m_vFallbackFaces.push_back(Face);
		return true;
	}

	bool SetVariantFaceByName(const char *pFamilyName)
	{
		FT_Face Face = GetFaceByName(pFamilyName);
		if(m_VariantFace != Face)
		{
			m_VariantFace = Face;
			Clear(); // rebuild atlas after changing variant font
			if(!Face && pFamilyName != nullptr)
			{
				log_error("textrender", "The variant font face '%s' could not be found", pFamilyName);
				return false;
			}
		}
		return true;
	}

	void SetFontPreset(EFontPreset FontPreset)
	{
		switch(FontPreset)
		{
		case EFontPreset::DEFAULT_FONT:
			m_SelectedFace = nullptr;
			break;
		case EFontPreset::ICON_FONT:
			m_SelectedFace = m_IconFace;
			break;
		}
	}

	void Clear()
	{
		for(size_t TextureIndex = 0; TextureIndex < NUM_FONT_TEXTURES; ++TextureIndex)
		{
			mem_zero(m_apTextureData[TextureIndex], m_TextureDimension * m_TextureDimension * sizeof(uint8_t));
			Graphics()->UpdateTextTexture(m_aTextures[TextureIndex], 0, 0, m_TextureDimension, m_TextureDimension, m_apTextureData[TextureIndex], false);
		}

		m_TextureAtlas.Clear(m_TextureDimension);
		m_Glyphs.clear();
	}

	const SGlyph *GetGlyph(int Chr, int FontSize)
	{
		FontSize = clamp(FontSize, MIN_FONT_SIZE, MAX_FONT_SIZE);

		// Find glyph index and most appropriate font face.
		FT_Face Face;
		FT_UInt GlyphIndex = GetCharGlyph(Chr, &Face, false);
		if(GlyphIndex == 0)
		{
			// Use replacement character if glyph could not be found,
			// also retrieve replacement character from the atlas.
			return Chr == REPLACEMENT_CHARACTER ? nullptr : GetGlyph(REPLACEMENT_CHARACTER, FontSize);
		}

		// Check if glyph for this (font face, character, font size)-combination was already rendered.
		SGlyph &Glyph = m_Glyphs[std::make_tuple(Face, Chr, FontSize)];
		if(Glyph.m_State == SGlyph::EState::RENDERED)
			return &Glyph;
		else if(Glyph.m_State == SGlyph::EState::ERROR)
			return nullptr;

		// Else, render it.
		Glyph.m_FontSize = FontSize;
		Glyph.m_Face = Face;
		Glyph.m_Chr = Chr;
		Glyph.m_GlyphIndex = GlyphIndex;
		if(RenderGlyph(Glyph))
			return &Glyph;

		// Use replacement character if the glyph could not be rendered,
		// also retrieve replacement character from the atlas.
		const SGlyph *pReplacementCharacter = Chr == REPLACEMENT_CHARACTER ? nullptr : GetGlyph(REPLACEMENT_CHARACTER, FontSize);
		if(pReplacementCharacter)
		{
			Glyph = *pReplacementCharacter;
			return &Glyph;
		}

		// Keep failed glyph in the cache so we don't attempt to render it again,
		// but set its state to ERROR so we don't return it to the text render.
		Glyph.m_State = SGlyph::EState::ERROR;
		return nullptr;
	}

	vec2 Kerning(const SGlyph *pLeft, const SGlyph *pRight) const
	{
		if(pLeft != nullptr && pRight != nullptr && pLeft->m_Face == pRight->m_Face && pLeft->m_FontSize == pRight->m_FontSize)
		{
			FT_Vector Kerning = {0, 0};
			FT_Set_Pixel_Sizes(pLeft->m_Face, 0, pLeft->m_FontSize);
			FT_Get_Kerning(pLeft->m_Face, pLeft->m_Chr, pRight->m_Chr, FT_KERNING_DEFAULT, &Kerning);
			return vec2(Kerning.x >> 6, Kerning.y >> 6);
		}
		return vec2(0.0f, 0.0f);
	}

	void UploadEntityLayerText(const CImageInfo &TextImage, int TexSubWidth, int TexSubHeight, const char *pText, int Length, float x, float y, int FontSize)
	{
		if(FontSize < 1)
			return;

		const size_t PixelSize = TextImage.PixelSize();
		const char *pCurrent = pText;
		const char *pEnd = pCurrent + Length;
		int WidthLastChars = 0;

		while(pCurrent < pEnd)
		{
			const char *pTmp = pCurrent;
			const int NextCharacter = str_utf8_decode(&pTmp);

			if(NextCharacter)
			{
				FT_Face Face;
				FT_UInt GlyphIndex = GetCharGlyph(NextCharacter, &Face, true);
				if(GlyphIndex == 0)
				{
					pCurrent = pTmp;
					continue;
				}

				FT_Set_Pixel_Sizes(Face, 0, FontSize);
				if(FT_Load_Char(Face, NextCharacter, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP))
				{
					log_debug("textrender", "Error loading glyph. Chr=%d GlyphIndex=%u", NextCharacter, GlyphIndex);
					pCurrent = pTmp;
					continue;
				}

				const FT_Bitmap *pBitmap = &Face->glyph->bitmap;
				if(pBitmap->pixel_mode != FT_PIXEL_MODE_GRAY)
				{
					log_debug("textrender", "Error loading glyph, unsupported pixel mode. Chr=%d GlyphIndex=%u PixelMode=%d", NextCharacter, GlyphIndex, pBitmap->pixel_mode);
					pCurrent = pTmp;
					continue;
				}

				for(unsigned OffY = 0; OffY < pBitmap->rows; ++OffY)
				{
					for(unsigned OffX = 0; OffX < pBitmap->width; ++OffX)
					{
						const int ImgOffX = clamp(x + OffX + WidthLastChars, x, (x + TexSubWidth) - 1);
						const int ImgOffY = clamp(y + OffY, y, (y + TexSubHeight) - 1);
						const size_t ImageOffset = ImgOffY * (TextImage.m_Width * PixelSize) + ImgOffX * PixelSize;
						for(size_t i = 0; i < PixelSize - 1; ++i)
						{
							TextImage.m_pData[ImageOffset + i] = 255;
						}
						TextImage.m_pData[ImageOffset + PixelSize - 1] = pBitmap->buffer[OffY * pBitmap->width + OffX];
					}
				}

				WidthLastChars += (pBitmap->width + 1);
			}
			pCurrent = pTmp;
		}
	}

	size_t TextureDimension() const
	{
		return m_TextureDimension;
	}

	IGraphics::CTextureHandle Texture(size_t TextureIndex) const
	{
		return m_aTextures[TextureIndex];
	}
};

typedef vector4_base<unsigned char> STextCharQuadVertexColor;

struct STextCharQuadVertex
{
	STextCharQuadVertex()
	{
		m_Color.r = m_Color.g = m_Color.b = m_Color.a = 255;
	}
	float m_X, m_Y;
	// do not use normalized floats as coordinates, since the texture might grow
	float m_U, m_V;
	STextCharQuadVertexColor m_Color;
};

struct STextCharQuad
{
	STextCharQuadVertex m_aVertices[4];
};

struct SStringInfo
{
	int m_QuadBufferObjectIndex;
	int m_QuadBufferContainerIndex;
	int m_SelectionQuadContainerIndex;

	std::vector<STextCharQuad> m_vCharacterQuads;
};

struct STextContainer
{
	STextContainer()
	{
		Reset();
	}

	SStringInfo m_StringInfo;

	// keep these values to calculate offsets
	float m_AlignedStartX;
	float m_AlignedStartY;
	float m_X;
	float m_Y;

	int m_Flags;
	int m_LineCount;
	int m_GlyphCount;
	int m_CharCount;
	int m_MaxLines;
	float m_LineWidth;

	unsigned m_RenderFlags;

	bool m_HasCursor;
	bool m_ForceCursorRendering;
	bool m_HasSelection;

	bool m_SingleTimeUse;

	STextBoundingBox m_BoundingBox;

	// prefix of the container's text stored for debugging purposes
	char m_aDebugText[32];

	STextContainerIndex m_ContainerIndex;

	void Reset()
	{
		m_StringInfo.m_QuadBufferObjectIndex = m_StringInfo.m_QuadBufferContainerIndex = m_StringInfo.m_SelectionQuadContainerIndex = -1;
		m_StringInfo.m_vCharacterQuads.clear();

		m_AlignedStartX = m_AlignedStartY = m_X = m_Y = 0.0f;
		m_Flags = m_LineCount = m_CharCount = m_GlyphCount = 0;
		m_MaxLines = -1;
		m_LineWidth = -1.0f;

		m_RenderFlags = 0;

		m_HasCursor = false;
		m_ForceCursorRendering = false;
		m_HasSelection = false;

		m_SingleTimeUse = false;

		m_BoundingBox = {0.0f, 0.0f, 0.0f, 0.0f};

		m_aDebugText[0] = '\0';

		m_ContainerIndex = STextContainerIndex{};
	}
};

struct SFontLanguageVariant
{
	char m_aLanguageFile[IO_MAX_PATH_LENGTH];
	char m_aFamilyName[FONT_NAME_SIZE];
};

class CTextRender : public IEngineTextRender
{
	IConsole *m_pConsole;
	IGraphics *m_pGraphics;
	IStorage *m_pStorage;
	IConsole *Console() { return m_pConsole; }
	IGraphics *Graphics() { return m_pGraphics; }
	IStorage *Storage() { return m_pStorage; }

	CGlyphMap *m_pGlyphMap;
	std::vector<void *> m_vpFontData;

	std::vector<SFontLanguageVariant> m_vVariants;

	unsigned m_RenderFlags;

	ColorRGBA m_Color;
	ColorRGBA m_OutlineColor;
	ColorRGBA m_SelectionColor;

	FT_Library m_FTLibrary;

	std::vector<STextContainer *> m_vpTextContainers;
	std::vector<int> m_vTextContainerIndices;
	int m_FirstFreeTextContainerIndex;

	SBufferContainerInfo m_DefaultTextContainerInfo;

	std::chrono::nanoseconds m_CursorRenderTime;

	int GetFreeTextContainerIndex()
	{
		if(m_FirstFreeTextContainerIndex == -1)
		{
			const int Index = (int)m_vTextContainerIndices.size();
			m_vTextContainerIndices.push_back(Index);
			return Index;
		}
		else
		{
			const int Index = m_FirstFreeTextContainerIndex;
			m_FirstFreeTextContainerIndex = m_vTextContainerIndices[Index];
			m_vTextContainerIndices[Index] = Index;
			return Index;
		}
	}

	void FreeTextContainerIndex(STextContainerIndex &Index)
	{
		m_vTextContainerIndices[Index.m_Index] = m_FirstFreeTextContainerIndex;
		m_FirstFreeTextContainerIndex = Index.m_Index;
		Index.Reset();
	}

	void FreeTextContainer(STextContainerIndex &Index)
	{
		m_vpTextContainers[Index.m_Index]->Reset();
		FreeTextContainerIndex(Index);
	}

	STextContainer &GetTextContainer(const STextContainerIndex &Index)
	{
		dbg_assert(Index.Valid(), "Text container index was invalid.");
		if(Index.m_Index >= (int)m_vpTextContainers.size())
		{
			for(int i = 0; i < Index.m_Index + 1 - (int)m_vpTextContainers.size(); ++i)
				m_vpTextContainers.push_back(new STextContainer());
		}

		if(m_vpTextContainers[Index.m_Index]->m_ContainerIndex.m_UseCount.get() != Index.m_UseCount.get())
		{
			m_vpTextContainers[Index.m_Index]->m_ContainerIndex = Index;
		}
		return *m_vpTextContainers[Index.m_Index];
	}

	int WordLength(const char *pText) const
	{
		const char *pCursor = pText;
		while(true)
		{
			if(*pCursor == '\0')
				return pCursor - pText;
			if(*pCursor == '\n' || *pCursor == '\t' || *pCursor == ' ')
				return pCursor - pText + 1;
			str_utf8_decode(&pCursor);
		}
	}

	bool LoadFontCollection(const char *pFontName, const FT_Byte *pFontData, FT_Long FontDataSize)
	{
		FT_Face FtFace;
		FT_Error CollectionLoadError = FT_New_Memory_Face(m_FTLibrary, pFontData, FontDataSize, -1, &FtFace);
		if(CollectionLoadError)
		{
			log_error("textrender", "Failed to load font file '%s': %s", pFontName, FT_Error_String(CollectionLoadError));
			return false;
		}

		const FT_Long NumFaces = FtFace->num_faces;
		FT_Done_Face(FtFace);

		bool LoadedAny = false;
		for(FT_Long FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
		{
			FT_Error FaceLoadError = FT_New_Memory_Face(m_FTLibrary, pFontData, FontDataSize, FaceIndex, &FtFace);
			if(FaceLoadError)
			{
				log_error("textrender", "Failed to load font face %ld from font file '%s': %s", FaceIndex, pFontName, FT_Error_String(FaceLoadError));
				FT_Done_Face(FtFace);
				continue;
			}

			m_pGlyphMap->AddFace(FtFace);

			log_debug("textrender", "Loaded font face %ld '%s %s' from font file '%s'", FaceIndex, FtFace->family_name, FtFace->style_name, pFontName);
			LoadedAny = true;
		}

		if(!LoadedAny)
		{
			log_error("textrender", "Failed to load font file '%s': no font faces could be loaded", pFontName);
			return false;
		}

		return true;
	}

	void SetRenderFlags(unsigned Flags) override
	{
		m_RenderFlags = Flags;
	}

	unsigned GetRenderFlags() const override
	{
		return m_RenderFlags;
	}

public:
	CTextRender()
	{
		m_pConsole = nullptr;
		m_pGraphics = nullptr;
		m_pStorage = nullptr;
		m_pGlyphMap = nullptr;

		m_Color = DefaultTextColor();
		m_OutlineColor = DefaultTextOutlineColor();
		m_SelectionColor = DefaultTextSelectionColor();

		m_FTLibrary = nullptr;

		m_RenderFlags = 0;
		m_CursorRenderTime = time_get_nanoseconds();
	}

	void Init() override
	{
		m_pConsole = Kernel()->RequestInterface<IConsole>();
		m_pGraphics = Kernel()->RequestInterface<IGraphics>();
		m_pStorage = Kernel()->RequestInterface<IStorage>();
		FT_Init_FreeType(&m_FTLibrary);
		m_pGlyphMap = new CGlyphMap(m_pGraphics);

		// print freetype version
		{
			int LMajor, LMinor, LPatch;
			FT_Library_Version(m_FTLibrary, &LMajor, &LMinor, &LPatch);
			log_info("textrender", "Freetype version %d.%d.%d (compiled = %d.%d.%d)", LMajor, LMinor, LPatch, FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH);
		}

		m_FirstFreeTextContainerIndex = -1;

		m_DefaultTextContainerInfo.m_Stride = sizeof(STextCharQuadVertex);
		m_DefaultTextContainerInfo.m_VertBufferBindingIndex = -1;

		m_DefaultTextContainerInfo.m_vAttributes.emplace_back();
		SBufferContainerInfo::SAttribute *pAttr = &m_DefaultTextContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 2;
		pAttr->m_FuncType = 0;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = nullptr;
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;

		m_DefaultTextContainerInfo.m_vAttributes.emplace_back();
		pAttr = &m_DefaultTextContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 2;
		pAttr->m_FuncType = 0;
		pAttr->m_Normalized = false;
		pAttr->m_pOffset = (void *)(sizeof(float) * 2);
		pAttr->m_Type = GRAPHICS_TYPE_FLOAT;

		m_DefaultTextContainerInfo.m_vAttributes.emplace_back();
		pAttr = &m_DefaultTextContainerInfo.m_vAttributes.back();
		pAttr->m_DataTypeCount = 4;
		pAttr->m_FuncType = 0;
		pAttr->m_Normalized = true;
		pAttr->m_pOffset = (void *)(sizeof(float) * 2 + sizeof(float) * 2);
		pAttr->m_Type = GRAPHICS_TYPE_UNSIGNED_BYTE;
	}

	void Shutdown() override
	{
		for(auto *pTextCont : m_vpTextContainers)
			delete pTextCont;
		m_vpTextContainers.clear();

		delete m_pGlyphMap;
		m_pGlyphMap = nullptr;

		if(m_FTLibrary != nullptr)
			FT_Done_FreeType(m_FTLibrary);
		m_FTLibrary = nullptr;

		for(auto *pFontData : m_vpFontData)
			free(pFontData);
		m_vpFontData.clear();

		m_DefaultTextContainerInfo.m_vAttributes.clear();

		m_pConsole = nullptr;
		m_pGraphics = nullptr;
		m_pStorage = nullptr;
	}

	bool LoadFonts() override
	{
		// read file data into buffer
		const char *pFilename = "fonts/index.json";
		void *pFileData;
		unsigned JsonFileSize;
		if(!Storage()->ReadFile(pFilename, IStorage::TYPE_ALL, &pFileData, &JsonFileSize))
		{
			log_error("textrender", "Failed to open/read font index file '%s'", pFilename);
			return false;
		}

		// parse json data
		json_settings JsonSettings{};
		char aError[256];
		json_value *pJsonData = json_parse_ex(&JsonSettings, static_cast<const json_char *>(pFileData), JsonFileSize, aError);
		free(pFileData);
		if(pJsonData == nullptr)
		{
			log_error("textrender", "Failed to parse font index file '%s': %s", pFilename, aError);
			return false;
		}
		if(pJsonData->type != json_object)
		{
			log_error("textrender", "Font index malformed: root must be an object", pFilename, aError);
			return false;
		}

		bool Success = true;

		// extract font file definitions
		const json_value &FontFiles = (*pJsonData)["font files"];
		if(FontFiles.type == json_array)
		{
			for(unsigned FontFileIndex = 0; FontFileIndex < FontFiles.u.array.length; ++FontFileIndex)
			{
				if(FontFiles[FontFileIndex].type != json_string)
				{
					log_error("textrender", "Font index malformed: 'font files' must be an array of strings (error at index %d)", FontFileIndex);
					Success = false;
					continue;
				}

				char aFontName[IO_MAX_PATH_LENGTH];
				str_format(aFontName, sizeof(aFontName), "fonts/%s", FontFiles[FontFileIndex].u.string.ptr);
				void *pFontData;
				unsigned FontDataSize;
				if(Storage()->ReadFile(aFontName, IStorage::TYPE_ALL, &pFontData, &FontDataSize))
				{
					if(LoadFontCollection(aFontName, static_cast<FT_Byte *>(pFontData), (FT_Long)FontDataSize))
					{
						m_vpFontData.push_back(pFontData);
					}
					else
					{
						free(pFontData);
					}
				}
				else
				{
					log_error("textrender", "Failed to open/read font file '%s'", aFontName);
					Success = false;
				}
			}
		}
		else
		{
			log_error("textrender", "Font index malformed: 'font files' must be an array");
			Success = false;
		}

		// extract default family name
		const json_value &DefaultFace = (*pJsonData)["default"];
		if(DefaultFace.type == json_string)
		{
			if(!m_pGlyphMap->SetDefaultFaceByName(DefaultFace.u.string.ptr))
			{
				Success = false;
			}
		}
		else
		{
			log_error("textrender", "Font index malformed: 'default' must be a string");
			Success = false;
		}

		// extract language variant family names
		const json_value &Variants = (*pJsonData)["language variants"];
		if(Variants.type == json_object)
		{
			m_vVariants.reserve(Variants.u.object.length);
			for(size_t i = 0; i < Variants.u.object.length; ++i)
			{
				const json_value *pFamilyName = Variants.u.object.values[i].value;
				if(pFamilyName->type != json_string)
				{
					log_error("textrender", "Font index malformed: 'language variants' entries must have string values (error on entry '%s')", Variants.u.object.values[i].name);
					Success = false;
					continue;
				}

				SFontLanguageVariant Variant;
				str_format(Variant.m_aLanguageFile, sizeof(Variant.m_aLanguageFile), "languages/%s.txt", Variants.u.object.values[i].name);
				str_copy(Variant.m_aFamilyName, pFamilyName->u.string.ptr);
				m_vVariants.emplace_back(Variant);
			}
		}
		else
		{
			log_error("textrender", "Font index malformed: 'language variants' must be an array");
			Success = false;
		}

		// extract fallback family names
		const json_value &FallbackFaces = (*pJsonData)["fallbacks"];
		if(FallbackFaces.type == json_array)
		{
			for(unsigned i = 0; i < FallbackFaces.u.array.length; ++i)
			{
				if(FallbackFaces[i].type != json_string)
				{
					log_error("textrender", "Font index malformed: 'fallbacks' must be an array of strings (error at index %d)", i);
					Success = false;
					continue;
				}
				if(!m_pGlyphMap->AddFallbackFaceByName(FallbackFaces[i].u.string.ptr))
				{
					Success = false;
				}
			}
		}
		else
		{
			log_error("textrender", "Font index malformed: 'fallbacks' must be an array");
			Success = false;
		}

		// extract icon font family name
		const json_value &IconFace = (*pJsonData)["icon"];
		if(IconFace.type == json_string)
		{
			if(!m_pGlyphMap->SetIconFaceByName(IconFace.u.string.ptr))
			{
				Success = false;
			}
		}
		else
		{
			log_error("textrender", "Font index malformed: 'icon' must be a string");
			Success = false;
		}

		json_value_free(pJsonData);
		return Success;
	}

	void SetFontPreset(EFontPreset FontPreset) override
	{
		m_pGlyphMap->SetFontPreset(FontPreset);
	}

	void SetFontLanguageVariant(const char *pLanguageFile) override
	{
		for(const auto &Variant : m_vVariants)
		{
			if(str_comp(pLanguageFile, Variant.m_aLanguageFile) == 0)
			{
				m_pGlyphMap->SetVariantFaceByName(Variant.m_aFamilyName);
				return;
			}
		}
		m_pGlyphMap->SetVariantFaceByName(nullptr);
	}

	void SetCursor(CTextCursor *pCursor, float x, float y, float FontSize, int Flags) const override
	{
		pCursor->m_Flags = Flags;
		pCursor->m_LineCount = 1;
		pCursor->m_GlyphCount = 0;
		pCursor->m_CharCount = 0;
		pCursor->m_MaxLines = 0;

		pCursor->m_LineSpacing = 0;
		pCursor->m_AlignedLineSpacing = 0;

		pCursor->m_StartX = x;
		pCursor->m_StartY = y;
		pCursor->m_LineWidth = -1.0f;
		pCursor->m_X = x;
		pCursor->m_Y = y;
		pCursor->m_MaxCharacterHeight = 0.0f;
		pCursor->m_LongestLineWidth = 0.0f;

		pCursor->m_FontSize = FontSize;
		pCursor->m_AlignedFontSize = FontSize;

		pCursor->m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_NONE;
		pCursor->m_SelectionHeightFactor = 1.0f;
		pCursor->m_PressMouse = vec2(0.0f, 0.0f);
		pCursor->m_ReleaseMouse = vec2(0.0f, 0.0f);
		pCursor->m_SelectionStart = 0;
		pCursor->m_SelectionEnd = 0;

		pCursor->m_CursorMode = TEXT_CURSOR_CURSOR_MODE_NONE;
		pCursor->m_ForceCursorRendering = false;
		pCursor->m_CursorCharacter = -1;
		pCursor->m_CursorRenderedPosition = vec2(-1.0f, -1.0f);

		pCursor->m_vColorSplits = {};
	}

	void MoveCursor(CTextCursor *pCursor, float x, float y) const override
	{
		pCursor->m_X += x;
		pCursor->m_Y += y;
	}

	void SetCursorPosition(CTextCursor *pCursor, float x, float y) const override
	{
		pCursor->m_X = x;
		pCursor->m_Y = y;
	}

	void Text(float x, float y, float Size, const char *pText, float LineWidth = -1.0f) override
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, x, y, Size, TEXTFLAG_RENDER);
		Cursor.m_LineWidth = LineWidth;
		TextEx(&Cursor, pText, -1);
	}

	float TextWidth(float Size, const char *pText, int StrLength = -1, float LineWidth = -1.0f, int Flags = 0, const STextSizeProperties &TextSizeProps = {}) override
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, Flags);
		Cursor.m_LineWidth = LineWidth;
		TextEx(&Cursor, pText, StrLength);
		if(TextSizeProps.m_pHeight != nullptr)
			*TextSizeProps.m_pHeight = Cursor.Height();
		if(TextSizeProps.m_pAlignedFontSize != nullptr)
			*TextSizeProps.m_pAlignedFontSize = Cursor.m_AlignedFontSize;
		if(TextSizeProps.m_pMaxCharacterHeightInLine != nullptr)
			*TextSizeProps.m_pMaxCharacterHeightInLine = Cursor.m_MaxCharacterHeight;
		if(TextSizeProps.m_pLineCount != nullptr)
			*TextSizeProps.m_pLineCount = Cursor.m_LineCount;
		return Cursor.m_LongestLineWidth;
	}

	STextBoundingBox TextBoundingBox(float Size, const char *pText, int StrLength = -1, float LineWidth = -1.0f, float LineSpacing = 0.0f, int Flags = 0) override
	{
		CTextCursor Cursor;
		SetCursor(&Cursor, 0, 0, Size, Flags);
		Cursor.m_LineWidth = LineWidth;
		Cursor.m_LineSpacing = LineSpacing;
		TextEx(&Cursor, pText, StrLength);
		return Cursor.BoundingBox();
	}

	void TextColor(float r, float g, float b, float a) override
	{
		m_Color.r = r;
		m_Color.g = g;
		m_Color.b = b;
		m_Color.a = a;
	}

	void TextColor(ColorRGBA Color) override
	{
		m_Color = Color;
	}

	void TextOutlineColor(float r, float g, float b, float a) override
	{
		m_OutlineColor.r = r;
		m_OutlineColor.g = g;
		m_OutlineColor.b = b;
		m_OutlineColor.a = a;
	}

	void TextOutlineColor(ColorRGBA Color) override
	{
		m_OutlineColor = Color;
	}

	void TextSelectionColor(float r, float g, float b, float a) override
	{
		m_SelectionColor.r = r;
		m_SelectionColor.g = g;
		m_SelectionColor.b = b;
		m_SelectionColor.a = a;
	}

	void TextSelectionColor(ColorRGBA Color) override
	{
		m_SelectionColor = Color;
	}

	ColorRGBA GetTextColor() const override
	{
		return m_Color;
	}

	ColorRGBA GetTextOutlineColor() const override
	{
		return m_OutlineColor;
	}

	ColorRGBA GetTextSelectionColor() const override
	{
		return m_SelectionColor;
	}

	void TextEx(CTextCursor *pCursor, const char *pText, int Length = -1) override
	{
		const unsigned OldRenderFlags = m_RenderFlags;
		m_RenderFlags |= TEXT_RENDER_FLAG_ONE_TIME_USE;
		STextContainerIndex TextCont;
		CreateTextContainer(TextCont, pCursor, pText, Length);
		m_RenderFlags = OldRenderFlags;
		if(TextCont.Valid())
		{
			if((pCursor->m_Flags & TEXTFLAG_RENDER) != 0)
			{
				ColorRGBA TextColor = DefaultTextColor();
				ColorRGBA TextColorOutline = DefaultTextOutlineColor();
				RenderTextContainer(TextCont, TextColor, TextColorOutline);
			}
			DeleteTextContainer(TextCont);
		}
	}

	bool CreateTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) override
	{
		dbg_assert(!TextContainerIndex.Valid(), "Text container index was not cleared.");

		TextContainerIndex.Reset();
		TextContainerIndex.m_Index = GetFreeTextContainerIndex();

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		STextContainer &TextContainer = GetTextContainer(TextContainerIndex);
		TextContainer.m_SingleTimeUse = (m_RenderFlags & TEXT_RENDER_FLAG_ONE_TIME_USE) != 0;
		const vec2 FakeToScreen = vec2(Graphics()->ScreenWidth() / (ScreenX1 - ScreenX0), Graphics()->ScreenHeight() / (ScreenY1 - ScreenY0));
		TextContainer.m_AlignedStartX = round_to_int(pCursor->m_X * FakeToScreen.x) / FakeToScreen.x;
		TextContainer.m_AlignedStartY = round_to_int(pCursor->m_Y * FakeToScreen.y) / FakeToScreen.y;
		TextContainer.m_X = pCursor->m_X;
		TextContainer.m_Y = pCursor->m_Y;
		TextContainer.m_Flags = pCursor->m_Flags;

		if(pCursor->m_LineWidth <= 0)
			TextContainer.m_RenderFlags = m_RenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE;
		else
			TextContainer.m_RenderFlags = m_RenderFlags;

		AppendTextContainer(TextContainerIndex, pCursor, pText, Length);

		const bool IsRendered = (pCursor->m_Flags & TEXTFLAG_RENDER) != 0;

		if(TextContainer.m_StringInfo.m_vCharacterQuads.empty() && TextContainer.m_StringInfo.m_SelectionQuadContainerIndex == -1 && IsRendered)
		{
			FreeTextContainer(TextContainerIndex);
			return false;
		}
		else
		{
			if(Graphics()->IsTextBufferingEnabled() && IsRendered && !TextContainer.m_StringInfo.m_vCharacterQuads.empty())
			{
				if((TextContainer.m_RenderFlags & TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD) == 0)
				{
					UploadTextContainer(TextContainerIndex);
				}
			}

			TextContainer.m_LineCount = pCursor->m_LineCount;
			TextContainer.m_GlyphCount = pCursor->m_GlyphCount;
			TextContainer.m_CharCount = pCursor->m_CharCount;
			TextContainer.m_MaxLines = pCursor->m_MaxLines;
			TextContainer.m_LineWidth = pCursor->m_LineWidth;
			return true;
		}
	}

	void AppendTextContainer(STextContainerIndex TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) override
	{
		STextContainer &TextContainer = GetTextContainer(TextContainerIndex);
		str_append(TextContainer.m_aDebugText, pText);

		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		const vec2 FakeToScreen = vec2(Graphics()->ScreenWidth() / (ScreenX1 - ScreenX0), Graphics()->ScreenHeight() / (ScreenY1 - ScreenY0));
		const float CursorX = round_to_int(pCursor->m_X * FakeToScreen.x) / FakeToScreen.x;
		const float CursorY = round_to_int(pCursor->m_Y * FakeToScreen.y) / FakeToScreen.y;
		const int ActualSize = round_truncate(pCursor->m_FontSize * FakeToScreen.y);
		pCursor->m_AlignedFontSize = ActualSize / FakeToScreen.y;
		pCursor->m_AlignedLineSpacing = round_truncate(pCursor->m_LineSpacing * FakeToScreen.y) / FakeToScreen.y;

		// string length
		if(Length < 0)
			Length = str_length(pText);
		else
			Length = minimum(Length, str_length(pText));

		const char *pCurrent = pText;
		const char *pEnd = pCurrent + Length;
		const char *pEllipsis = "…";
		const SGlyph *pEllipsisGlyph = nullptr;
		if(pCursor->m_Flags & TEXTFLAG_ELLIPSIS_AT_END)
		{
			if(pCursor->m_LineWidth != -1 && pCursor->m_LineWidth < TextWidth(pCursor->m_FontSize, pText, -1, -1.0f))
			{
				pEllipsisGlyph = m_pGlyphMap->GetGlyph(0x2026, ActualSize); // …
				if(pEllipsisGlyph == nullptr)
				{
					// no ellipsis char in font, just stop at end instead
					pCursor->m_Flags &= ~TEXTFLAG_ELLIPSIS_AT_END;
					pCursor->m_Flags |= TEXTFLAG_STOP_AT_END;
				}
			}
		}

		const unsigned RenderFlags = TextContainer.m_RenderFlags;

		float DrawX = 0.0f, DrawY = 0.0f;
		if((RenderFlags & TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT) != 0)
		{
			DrawX = pCursor->m_X;
			DrawY = pCursor->m_Y;
		}
		else
		{
			DrawX = CursorX;
			DrawY = CursorY;
		}

		int LineCount = pCursor->m_LineCount;

		const bool IsRendered = (pCursor->m_Flags & TEXTFLAG_RENDER) != 0;

		const float CursorInnerWidth = (((ScreenX1 - ScreenX0) / Graphics()->ScreenWidth())) * 2;
		const float CursorOuterWidth = CursorInnerWidth * 2;
		const float CursorOuterInnerDiff = (CursorOuterWidth - CursorInnerWidth) / 2;

		std::vector<IGraphics::CQuadItem> vSelectionQuads;
		int SelectionQuadLine = -1;
		bool SelectionStarted = false;
		bool SelectionUsedPress = false;
		bool SelectionUsedRelease = false;
		int SelectionStartChar = -1;
		int SelectionEndChar = -1;

		const auto &&CheckInsideChar = [&](bool CheckOuter, vec2 CursorPos, float LastCharX, float LastCharWidth, float CharX, float CharWidth, float CharY) -> bool {
			return (LastCharX - LastCharWidth / 2 <= CursorPos.x &&
				       CharX + CharWidth / 2 > CursorPos.x &&
				       CursorPos.y >= CharY - pCursor->m_AlignedFontSize &&
				       CursorPos.y < CharY + pCursor->m_AlignedLineSpacing) ||
			       (CheckOuter &&
				       CursorPos.y <= CharY - pCursor->m_AlignedFontSize);
		};
		const auto &&CheckSelectionStart = [&](bool CheckOuter, vec2 CursorPos, int &SelectionChar, bool &SelectionUsedCase, float LastCharX, float LastCharWidth, float CharX, float CharWidth, float CharY) {
			if(!SelectionStarted && !SelectionUsedCase &&
				CheckInsideChar(CheckOuter, CursorPos, LastCharX, LastCharWidth, CharX, CharWidth, CharY))
			{
				SelectionChar = pCursor->m_GlyphCount;
				SelectionStarted = !SelectionStarted;
				SelectionUsedCase = true;
			}
		};
		const auto &&CheckOutsideChar = [&](bool CheckOuter, vec2 CursorPos, float CharX, float CharWidth, float CharY) -> bool {
			return (CharX + CharWidth / 2 > CursorPos.x &&
				       CursorPos.y >= CharY - pCursor->m_AlignedFontSize &&
				       CursorPos.y < CharY + pCursor->m_AlignedLineSpacing) ||
			       (CheckOuter &&
				       CursorPos.y >= CharY + pCursor->m_AlignedLineSpacing);
		};
		const auto &&CheckSelectionEnd = [&](bool CheckOuter, vec2 CursorPos, int &SelectionChar, bool &SelectionUsedCase, float CharX, float CharWidth, float CharY) {
			if(SelectionStarted && !SelectionUsedCase &&
				CheckOutsideChar(CheckOuter, CursorPos, CharX, CharWidth, CharY))
			{
				SelectionChar = pCursor->m_GlyphCount;
				SelectionStarted = !SelectionStarted;
				SelectionUsedCase = true;
			}
		};

		float LastSelX = DrawX;
		float LastSelWidth = 0;
		float LastCharX = DrawX;
		float LastCharWidth = 0;

		// Returns true if line was started
		const auto &&StartNewLine = [&]() {
			if(pCursor->m_MaxLines > 0 && LineCount >= pCursor->m_MaxLines)
				return false;

			DrawX = pCursor->m_StartX;
			DrawY += pCursor->m_AlignedFontSize + pCursor->m_AlignedLineSpacing;
			if((RenderFlags & TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT) == 0)
			{
				DrawX = round_to_int(DrawX * FakeToScreen.x) / FakeToScreen.x; // realign
				DrawY = round_to_int(DrawY * FakeToScreen.y) / FakeToScreen.y;
			}
			LastSelX = DrawX;
			LastSelWidth = 0;
			LastCharX = DrawX;
			LastCharWidth = 0;
			++LineCount;
			return true;
		};

		if(pCursor->m_CalculateSelectionMode != TEXT_CURSOR_SELECTION_MODE_NONE || pCursor->m_CursorMode != TEXT_CURSOR_CURSOR_MODE_NONE)
		{
			if(IsRendered)
				Graphics()->QuadContainerReset(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex);

			// if in calculate mode, also calculate the cursor
			if(pCursor->m_CursorMode == TEXT_CURSOR_CURSOR_MODE_CALCULATE)
				pCursor->m_CursorCharacter = -1;
		}

		IGraphics::CQuadItem aCursorQuads[2];
		bool HasCursor = false;

		const SGlyph *pLastGlyph = nullptr;
		bool GotNewLineLast = false;

		int ColorOption = 0;

		while(pCurrent < pEnd && pCurrent != pEllipsis)
		{
			bool NewLine = false;
			const char *pBatchEnd = pEnd;
			if(pCursor->m_LineWidth > 0 && !(pCursor->m_Flags & TEXTFLAG_STOP_AT_END) && !(pCursor->m_Flags & TEXTFLAG_ELLIPSIS_AT_END))
			{
				int Wlen = minimum(WordLength(pCurrent), (int)(pEnd - pCurrent));
				CTextCursor Compare = *pCursor;
				Compare.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_NONE;
				Compare.m_CursorMode = TEXT_CURSOR_CURSOR_MODE_NONE;
				Compare.m_X = DrawX;
				Compare.m_Y = DrawY;
				Compare.m_Flags &= ~TEXTFLAG_RENDER;
				Compare.m_Flags |= TEXTFLAG_DISALLOW_NEWLINE;
				Compare.m_LineWidth = -1;
				TextEx(&Compare, pCurrent, Wlen);

				if(Compare.m_X - DrawX > pCursor->m_LineWidth)
				{
					// word can't be fitted in one line, cut it
					CTextCursor Cutter = *pCursor;
					Cutter.m_CalculateSelectionMode = TEXT_CURSOR_SELECTION_MODE_NONE;
					Cutter.m_CursorMode = TEXT_CURSOR_CURSOR_MODE_NONE;
					Cutter.m_GlyphCount = 0;
					Cutter.m_CharCount = 0;
					Cutter.m_X = DrawX;
					Cutter.m_Y = DrawY;
					Cutter.m_Flags &= ~TEXTFLAG_RENDER;
					Cutter.m_Flags |= TEXTFLAG_STOP_AT_END | TEXTFLAG_DISALLOW_NEWLINE;

					TextEx(&Cutter, pCurrent, Wlen);
					Wlen = str_utf8_rewind(pCurrent, Cutter.m_CharCount); // rewind once to skip the last character that did not fit
					NewLine = true;

					if(Cutter.m_GlyphCount <= 3 && !GotNewLineLast) // if we can't place 3 chars of the word on this line, take the next
						Wlen = 0;
				}
				else if(Compare.m_X - pCursor->m_StartX > pCursor->m_LineWidth && !GotNewLineLast)
				{
					NewLine = true;
					Wlen = 0;
				}

				pBatchEnd = pCurrent + Wlen;
			}

			const char *pTmp = pCurrent;
			int NextCharacter = str_utf8_decode(&pTmp);

			while(pCurrent < pBatchEnd && pCurrent != pEllipsis)
			{
				const int PrevCharCount = pCursor->m_CharCount;
				pCursor->m_CharCount += pTmp - pCurrent;
				pCurrent = pTmp;
				int Character = NextCharacter;
				NextCharacter = str_utf8_decode(&pTmp);

				if(Character == '\n')
				{
					if((pCursor->m_Flags & TEXTFLAG_DISALLOW_NEWLINE) == 0)
					{
						pLastGlyph = nullptr;
						if(!StartNewLine())
							break;
						continue;
					}
					else
					{
						Character = ' ';
					}
				}

				const SGlyph *pGlyph = m_pGlyphMap->GetGlyph(Character, ActualSize);
				if(pGlyph)
				{
					const float Scale = 1.0f / pGlyph->m_FontSize;

					const bool ApplyBearingX = !(((RenderFlags & TEXT_RENDER_FLAG_NO_X_BEARING) != 0) || (pCursor->m_GlyphCount == 0 && (RenderFlags & TEXT_RENDER_FLAG_NO_FIRST_CHARACTER_X_BEARING) != 0));
					const float Advance = ((((RenderFlags & TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH) != 0) ? (pGlyph->m_Width) : (pGlyph->m_AdvanceX + ((!ApplyBearingX) ? (-pGlyph->m_OffsetX) : 0.f)))) * Scale * pCursor->m_AlignedFontSize;

					const float OutLineRealDiff = (pGlyph->m_Width - pGlyph->m_CharWidth) * Scale * pCursor->m_AlignedFontSize;

					float CharKerning = 0.0f;
					if((RenderFlags & TEXT_RENDER_FLAG_KERNING) != 0)
						CharKerning = m_pGlyphMap->Kerning(pLastGlyph, pGlyph).x * Scale * pCursor->m_AlignedFontSize;
					pLastGlyph = pGlyph;

					if(pEllipsisGlyph != nullptr && pCursor->m_Flags & TEXTFLAG_ELLIPSIS_AT_END && pCurrent < pBatchEnd && pCurrent != pEllipsis)
					{
						float AdvanceEllipsis = ((((RenderFlags & TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH) != 0) ? (pEllipsisGlyph->m_Width) : (pEllipsisGlyph->m_AdvanceX + ((!ApplyBearingX) ? (-pEllipsisGlyph->m_OffsetX) : 0.f)))) * Scale * pCursor->m_AlignedFontSize;
						float CharKerningEllipsis = 0.0f;
						if((RenderFlags & TEXT_RENDER_FLAG_KERNING) != 0)
						{
							CharKerningEllipsis = m_pGlyphMap->Kerning(pGlyph, pEllipsisGlyph).x * Scale * pCursor->m_AlignedFontSize;
						}
						if(DrawX + CharKerning + Advance + CharKerningEllipsis + AdvanceEllipsis - pCursor->m_StartX > pCursor->m_LineWidth)
						{
							// we hit the end, only render ellipsis and finish
							pTmp = pEllipsis;
							NextCharacter = 0x2026;
							continue;
						}
					}

					if(pCursor->m_Flags & TEXTFLAG_STOP_AT_END && (DrawX + CharKerning) + Advance - pCursor->m_StartX > pCursor->m_LineWidth)
					{
						// we hit the end of the line, no more to render or count
						pCurrent = pEnd;
						break;
					}

					float BearingX = (!ApplyBearingX ? 0.f : pGlyph->m_OffsetX) * Scale * pCursor->m_AlignedFontSize;
					float CharWidth = pGlyph->m_Width * Scale * pCursor->m_AlignedFontSize;

					float BearingY = (((RenderFlags & TEXT_RENDER_FLAG_NO_Y_BEARING) != 0) ? 0.f : (pGlyph->m_OffsetY * Scale * pCursor->m_AlignedFontSize));
					float CharHeight = pGlyph->m_Height * Scale * pCursor->m_AlignedFontSize;

					if((RenderFlags & TEXT_RENDER_FLAG_NO_OVERSIZE) != 0)
					{
						if(CharHeight + BearingY > pCursor->m_AlignedFontSize)
						{
							BearingY = 0;
							float ScaleChar = (CharHeight + BearingY) / pCursor->m_AlignedFontSize;
							CharHeight = pCursor->m_AlignedFontSize;
							CharWidth /= ScaleChar;
						}
					}

					const float TmpY = (DrawY + pCursor->m_AlignedFontSize);
					const float CharX = (DrawX + CharKerning) + BearingX;
					const float CharY = TmpY - BearingY;

					// Check if we have any color split
					ColorRGBA Color = m_Color;
					if(ColorOption < (int)pCursor->m_vColorSplits.size())
					{
						STextColorSplit &Split = pCursor->m_vColorSplits.at(ColorOption);
						if(PrevCharCount >= Split.m_CharIndex && (Split.m_Length == -1 || PrevCharCount < Split.m_CharIndex + Split.m_Length))
							Color = Split.m_Color;
						if(Split.m_Length != -1 && PrevCharCount >= (Split.m_CharIndex + Split.m_Length - 1))
						{
							ColorOption++;
							if(ColorOption < (int)pCursor->m_vColorSplits.size())
							{ // Handle splits that are
								Split = pCursor->m_vColorSplits.at(ColorOption);
								if(PrevCharCount >= Split.m_CharIndex)
									Color = Split.m_Color;
							}
						}
					}

					// don't add text that isn't drawn, the color overwrite is used for that
					if(Color.a != 0.f && IsRendered)
					{
						TextContainer.m_StringInfo.m_vCharacterQuads.emplace_back();
						STextCharQuad &TextCharQuad = TextContainer.m_StringInfo.m_vCharacterQuads.back();

						TextCharQuad.m_aVertices[0].m_X = CharX;
						TextCharQuad.m_aVertices[0].m_Y = CharY;
						TextCharQuad.m_aVertices[0].m_U = pGlyph->m_aUVs[0];
						TextCharQuad.m_aVertices[0].m_V = pGlyph->m_aUVs[3];
						TextCharQuad.m_aVertices[0].m_Color.r = (unsigned char)(Color.r * 255.f);
						TextCharQuad.m_aVertices[0].m_Color.g = (unsigned char)(Color.g * 255.f);
						TextCharQuad.m_aVertices[0].m_Color.b = (unsigned char)(Color.b * 255.f);
						TextCharQuad.m_aVertices[0].m_Color.a = (unsigned char)(Color.a * 255.f);

						TextCharQuad.m_aVertices[1].m_X = CharX + CharWidth;
						TextCharQuad.m_aVertices[1].m_Y = CharY;
						TextCharQuad.m_aVertices[1].m_U = pGlyph->m_aUVs[2];
						TextCharQuad.m_aVertices[1].m_V = pGlyph->m_aUVs[3];
						TextCharQuad.m_aVertices[1].m_Color.r = (unsigned char)(Color.r * 255.f);
						TextCharQuad.m_aVertices[1].m_Color.g = (unsigned char)(Color.g * 255.f);
						TextCharQuad.m_aVertices[1].m_Color.b = (unsigned char)(Color.b * 255.f);
						TextCharQuad.m_aVertices[1].m_Color.a = (unsigned char)(Color.a * 255.f);

						TextCharQuad.m_aVertices[2].m_X = CharX + CharWidth;
						TextCharQuad.m_aVertices[2].m_Y = CharY - CharHeight;
						TextCharQuad.m_aVertices[2].m_U = pGlyph->m_aUVs[2];
						TextCharQuad.m_aVertices[2].m_V = pGlyph->m_aUVs[1];
						TextCharQuad.m_aVertices[2].m_Color.r = (unsigned char)(Color.r * 255.f);
						TextCharQuad.m_aVertices[2].m_Color.g = (unsigned char)(Color.g * 255.f);
						TextCharQuad.m_aVertices[2].m_Color.b = (unsigned char)(Color.b * 255.f);
						TextCharQuad.m_aVertices[2].m_Color.a = (unsigned char)(Color.a * 255.f);

						TextCharQuad.m_aVertices[3].m_X = CharX;
						TextCharQuad.m_aVertices[3].m_Y = CharY - CharHeight;
						TextCharQuad.m_aVertices[3].m_U = pGlyph->m_aUVs[0];
						TextCharQuad.m_aVertices[3].m_V = pGlyph->m_aUVs[1];
						TextCharQuad.m_aVertices[3].m_Color.r = (unsigned char)(Color.r * 255.f);
						TextCharQuad.m_aVertices[3].m_Color.g = (unsigned char)(Color.g * 255.f);
						TextCharQuad.m_aVertices[3].m_Color.b = (unsigned char)(Color.b * 255.f);
						TextCharQuad.m_aVertices[3].m_Color.a = (unsigned char)(Color.a * 255.f);
					}

					// calculate the full width from the last selection point to the end of this selection draw on screen
					const float SelWidth = (CharX + maximum(Advance, CharWidth - OutLineRealDiff / 2)) - (LastSelX + LastSelWidth);
					const float SelX = (LastSelX + LastSelWidth);

					if(pCursor->m_CursorMode == TEXT_CURSOR_CURSOR_MODE_CALCULATE)
					{
						if(pCursor->m_CursorCharacter == -1 && CheckInsideChar(pCursor->m_GlyphCount == 0, pCursor->m_ReleaseMouse, pCursor->m_GlyphCount == 0 ? std::numeric_limits<float>::lowest() : LastCharX, LastCharWidth, CharX, CharWidth, TmpY))
						{
							pCursor->m_CursorCharacter = pCursor->m_GlyphCount;
						}
					}

					if(pCursor->m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE)
					{
						if(pCursor->m_GlyphCount == 0)
						{
							CheckSelectionStart(true, pCursor->m_PressMouse, SelectionStartChar, SelectionUsedPress, std::numeric_limits<float>::lowest(), 0, CharX, CharWidth, TmpY);
							CheckSelectionStart(true, pCursor->m_ReleaseMouse, SelectionEndChar, SelectionUsedRelease, std::numeric_limits<float>::lowest(), 0, CharX, CharWidth, TmpY);
						}

						// if selection didn't start and the mouse pos is at least on 50% of the right side of the character start
						CheckSelectionStart(false, pCursor->m_PressMouse, SelectionStartChar, SelectionUsedPress, LastCharX, LastCharWidth, CharX, CharWidth, TmpY);
						CheckSelectionStart(false, pCursor->m_ReleaseMouse, SelectionEndChar, SelectionUsedRelease, LastCharX, LastCharWidth, CharX, CharWidth, TmpY);
						CheckSelectionEnd(false, pCursor->m_ReleaseMouse, SelectionEndChar, SelectionUsedRelease, CharX, CharWidth, TmpY);
						CheckSelectionEnd(false, pCursor->m_PressMouse, SelectionStartChar, SelectionUsedPress, CharX, CharWidth, TmpY);
					}
					if(pCursor->m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_SET)
					{
						if(pCursor->m_GlyphCount == pCursor->m_SelectionStart)
						{
							SelectionStarted = !SelectionStarted;
							SelectionStartChar = pCursor->m_GlyphCount;
							SelectionUsedPress = true;
						}
						if(pCursor->m_GlyphCount == pCursor->m_SelectionEnd)
						{
							SelectionStarted = !SelectionStarted;
							SelectionEndChar = pCursor->m_GlyphCount;
							SelectionUsedRelease = true;
						}
					}

					if(pCursor->m_CursorMode != TEXT_CURSOR_CURSOR_MODE_NONE)
					{
						if(pCursor->m_GlyphCount == pCursor->m_CursorCharacter)
						{
							HasCursor = true;
							aCursorQuads[0] = IGraphics::CQuadItem(SelX - CursorOuterInnerDiff, DrawY, CursorOuterWidth, pCursor->m_AlignedFontSize);
							aCursorQuads[1] = IGraphics::CQuadItem(SelX, DrawY + CursorOuterInnerDiff, CursorInnerWidth, pCursor->m_AlignedFontSize - CursorOuterInnerDiff * 2);
							pCursor->m_CursorRenderedPosition = vec2(SelX, DrawY);
						}
					}

					pCursor->m_MaxCharacterHeight = maximum(pCursor->m_MaxCharacterHeight, CharHeight + BearingY);

					if(NextCharacter == 0 && (RenderFlags & TEXT_RENDER_FLAG_NO_LAST_CHARACTER_ADVANCE) != 0 && Character != ' ')
						DrawX += BearingX + CharKerning + CharWidth;
					else
						DrawX += Advance + CharKerning;

					pCursor->m_GlyphCount++;

					if(SelectionStarted && IsRendered)
					{
						if(!vSelectionQuads.empty() && SelectionQuadLine == LineCount)
						{
							vSelectionQuads.back().m_Width += SelWidth;
						}
						else
						{
							const float SelectionHeight = pCursor->m_AlignedFontSize + pCursor->m_AlignedLineSpacing;
							const float SelectionY = DrawY + (1.0f - pCursor->m_SelectionHeightFactor) * SelectionHeight;
							const float ScaledSelectionHeight = pCursor->m_SelectionHeightFactor * SelectionHeight;
							vSelectionQuads.emplace_back(SelX, SelectionY, SelWidth, ScaledSelectionHeight);
							SelectionQuadLine = LineCount;
						}
					}

					LastSelX = SelX;
					LastSelWidth = SelWidth;
					LastCharX = CharX;
					LastCharWidth = CharWidth;
				}

				pCursor->m_LongestLineWidth = maximum(pCursor->m_LongestLineWidth, DrawX - pCursor->m_StartX);
			}

			if(NewLine)
			{
				if(!StartNewLine())
					break;
				GotNewLineLast = true;
			}
			else
				GotNewLineLast = false;
		}

		if(!TextContainer.m_StringInfo.m_vCharacterQuads.empty() && IsRendered)
		{
			// setup the buffers
			if(Graphics()->IsTextBufferingEnabled())
			{
				const size_t DataSize = TextContainer.m_StringInfo.m_vCharacterQuads.size() * sizeof(STextCharQuad);
				void *pUploadData = TextContainer.m_StringInfo.m_vCharacterQuads.data();

				if(TextContainer.m_StringInfo.m_QuadBufferObjectIndex != -1 && (TextContainer.m_RenderFlags & TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD) == 0)
				{
					Graphics()->RecreateBufferObject(TextContainer.m_StringInfo.m_QuadBufferObjectIndex, DataSize, pUploadData, TextContainer.m_SingleTimeUse ? IGraphics::EBufferObjectCreateFlags::BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT : 0);
					Graphics()->IndicesNumRequiredNotify(TextContainer.m_StringInfo.m_vCharacterQuads.size() * 6);
				}
			}
		}

		if(pCursor->m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_CALCULATE)
		{
			pCursor->m_SelectionStart = -1;
			pCursor->m_SelectionEnd = -1;

			if(SelectionStarted)
			{
				CheckSelectionEnd(true, pCursor->m_ReleaseMouse, SelectionEndChar, SelectionUsedRelease, std::numeric_limits<float>::max(), 0, DrawY + pCursor->m_AlignedFontSize);
				CheckSelectionEnd(true, pCursor->m_PressMouse, SelectionStartChar, SelectionUsedPress, std::numeric_limits<float>::max(), 0, DrawY + pCursor->m_AlignedFontSize);
			}
		}
		else if(pCursor->m_CalculateSelectionMode == TEXT_CURSOR_SELECTION_MODE_SET)
		{
			if(pCursor->m_GlyphCount == pCursor->m_SelectionStart)
			{
				SelectionStarted = !SelectionStarted;
				SelectionStartChar = pCursor->m_GlyphCount;
				SelectionUsedPress = true;
			}
			if(pCursor->m_GlyphCount == pCursor->m_SelectionEnd)
			{
				SelectionStarted = !SelectionStarted;
				SelectionEndChar = pCursor->m_GlyphCount;
				SelectionUsedRelease = true;
			}
		}

		if(pCursor->m_CursorMode != TEXT_CURSOR_CURSOR_MODE_NONE)
		{
			if(pCursor->m_CursorMode == TEXT_CURSOR_CURSOR_MODE_CALCULATE && pCursor->m_CursorCharacter == -1 && CheckOutsideChar(true, pCursor->m_ReleaseMouse, std::numeric_limits<float>::max(), 0, DrawY + pCursor->m_AlignedFontSize))
			{
				pCursor->m_CursorCharacter = pCursor->m_GlyphCount;
			}

			if(pCursor->m_GlyphCount == pCursor->m_CursorCharacter)
			{
				HasCursor = true;
				aCursorQuads[0] = IGraphics::CQuadItem((LastSelX + LastSelWidth) - CursorOuterInnerDiff, DrawY, CursorOuterWidth, pCursor->m_AlignedFontSize);
				aCursorQuads[1] = IGraphics::CQuadItem((LastSelX + LastSelWidth), DrawY + CursorOuterInnerDiff, CursorInnerWidth, pCursor->m_AlignedFontSize - CursorOuterInnerDiff * 2);
				pCursor->m_CursorRenderedPosition = vec2(LastSelX + LastSelWidth, DrawY);
			}
		}

		const bool HasSelection = !vSelectionQuads.empty() && SelectionUsedPress && SelectionUsedRelease;
		if((HasSelection || HasCursor) && IsRendered)
		{
			Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
			if(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex == -1)
				TextContainer.m_StringInfo.m_SelectionQuadContainerIndex = Graphics()->CreateQuadContainer(false);
			if(HasCursor)
				Graphics()->QuadContainerAddQuads(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, aCursorQuads, std::size(aCursorQuads));
			if(HasSelection)
				Graphics()->QuadContainerAddQuads(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, vSelectionQuads.data(), vSelectionQuads.size());
			Graphics()->QuadContainerUpload(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex);

			TextContainer.m_HasCursor = HasCursor;
			TextContainer.m_HasSelection = HasSelection;
			TextContainer.m_ForceCursorRendering = pCursor->m_ForceCursorRendering;

			if(HasSelection)
			{
				pCursor->m_SelectionStart = SelectionStartChar;
				pCursor->m_SelectionEnd = SelectionEndChar;
			}
			else
			{
				pCursor->m_SelectionStart = -1;
				pCursor->m_SelectionEnd = -1;
			}
		}

		// even if no text is drawn the cursor position will be adjusted
		pCursor->m_X = DrawX;
		pCursor->m_Y = DrawY;
		pCursor->m_LineCount = LineCount;

		TextContainer.m_BoundingBox = pCursor->BoundingBox();
	}

	bool CreateOrAppendTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) override
	{
		if(TextContainerIndex.Valid())
		{
			AppendTextContainer(TextContainerIndex, pCursor, pText, Length);
			return true;
		}
		else
		{
			return CreateTextContainer(TextContainerIndex, pCursor, pText, Length);
		}
	}

	// just deletes and creates text container
	void RecreateTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) override
	{
		DeleteTextContainer(TextContainerIndex);
		CreateTextContainer(TextContainerIndex, pCursor, pText, Length);
	}

	void RecreateTextContainerSoft(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) override
	{
		STextContainer &TextContainer = GetTextContainer(TextContainerIndex);
		TextContainer.m_StringInfo.m_vCharacterQuads.clear();
		// the text buffer gets then recreated by the appended quads
		AppendTextContainer(TextContainerIndex, pCursor, pText, Length);
	}

	void DeleteTextContainer(STextContainerIndex &TextContainerIndex) override
	{
		if(!TextContainerIndex.Valid())
			return;

		STextContainer &TextContainer = GetTextContainer(TextContainerIndex);
		if(Graphics()->IsTextBufferingEnabled())
			Graphics()->DeleteBufferContainer(TextContainer.m_StringInfo.m_QuadBufferContainerIndex, true);
		Graphics()->DeleteQuadContainer(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex);
		FreeTextContainer(TextContainerIndex);
	}

	void UploadTextContainer(STextContainerIndex TextContainerIndex) override
	{
		if(Graphics()->IsTextBufferingEnabled())
		{
			STextContainer &TextContainer = GetTextContainer(TextContainerIndex);
			size_t DataSize = TextContainer.m_StringInfo.m_vCharacterQuads.size() * sizeof(STextCharQuad);
			void *pUploadData = TextContainer.m_StringInfo.m_vCharacterQuads.data();
			TextContainer.m_StringInfo.m_QuadBufferObjectIndex = Graphics()->CreateBufferObject(DataSize, pUploadData, TextContainer.m_SingleTimeUse ? IGraphics::EBufferObjectCreateFlags::BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT : 0);

			m_DefaultTextContainerInfo.m_VertBufferBindingIndex = TextContainer.m_StringInfo.m_QuadBufferObjectIndex;

			TextContainer.m_StringInfo.m_QuadBufferContainerIndex = Graphics()->CreateBufferContainer(&m_DefaultTextContainerInfo);
			Graphics()->IndicesNumRequiredNotify(TextContainer.m_StringInfo.m_vCharacterQuads.size() * 6);
		}
	}

	void RenderTextContainer(STextContainerIndex TextContainerIndex, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor) override
	{
		const STextContainer &TextContainer = GetTextContainer(TextContainerIndex);

		if(!TextContainer.m_StringInfo.m_vCharacterQuads.empty())
		{
			if(Graphics()->IsTextBufferingEnabled())
			{
				Graphics()->TextureClear();
				// render buffered text
				Graphics()->RenderText(TextContainer.m_StringInfo.m_QuadBufferContainerIndex, TextContainer.m_StringInfo.m_vCharacterQuads.size(), m_pGlyphMap->TextureDimension(), m_pGlyphMap->Texture(CGlyphMap::FONT_TEXTURE_FILL).Id(), m_pGlyphMap->Texture(CGlyphMap::FONT_TEXTURE_OUTLINE).Id(), TextColor, TextOutlineColor);
			}
			else
			{
				// render tiles
				const float UVScale = 1.0f / m_pGlyphMap->TextureDimension();

				Graphics()->FlushVertices();
				Graphics()->TextureSet(m_pGlyphMap->Texture(CGlyphMap::FONT_TEXTURE_OUTLINE));

				Graphics()->QuadsBegin();

				for(const STextCharQuad &TextCharQuad : TextContainer.m_StringInfo.m_vCharacterQuads)
				{
					Graphics()->SetColor(TextCharQuad.m_aVertices[0].m_Color.r / 255.f * TextOutlineColor.r, TextCharQuad.m_aVertices[0].m_Color.g / 255.f * TextOutlineColor.g, TextCharQuad.m_aVertices[0].m_Color.b / 255.f * TextOutlineColor.b, TextCharQuad.m_aVertices[0].m_Color.a / 255.f * TextOutlineColor.a);
					Graphics()->QuadsSetSubset(TextCharQuad.m_aVertices[0].m_U * UVScale, TextCharQuad.m_aVertices[0].m_V * UVScale, TextCharQuad.m_aVertices[2].m_U * UVScale, TextCharQuad.m_aVertices[2].m_V * UVScale);
					IGraphics::CQuadItem QuadItem(TextCharQuad.m_aVertices[0].m_X, TextCharQuad.m_aVertices[0].m_Y, TextCharQuad.m_aVertices[1].m_X - TextCharQuad.m_aVertices[0].m_X, TextCharQuad.m_aVertices[2].m_Y - TextCharQuad.m_aVertices[0].m_Y);
					Graphics()->QuadsDrawTL(&QuadItem, 1);
				}

				if(TextColor.a != 0)
				{
					Graphics()->QuadsEndKeepVertices();
					Graphics()->TextureSet(m_pGlyphMap->Texture(CGlyphMap::FONT_TEXTURE_FILL));

					int TextCharQuadIndex = 0;
					for(const STextCharQuad &TextCharQuad : TextContainer.m_StringInfo.m_vCharacterQuads)
					{
						unsigned char CR = (unsigned char)((float)(TextCharQuad.m_aVertices[0].m_Color.r) * TextColor.r);
						unsigned char CG = (unsigned char)((float)(TextCharQuad.m_aVertices[0].m_Color.g) * TextColor.g);
						unsigned char CB = (unsigned char)((float)(TextCharQuad.m_aVertices[0].m_Color.b) * TextColor.b);
						unsigned char CA = (unsigned char)((float)(TextCharQuad.m_aVertices[0].m_Color.a) * TextColor.a);
						Graphics()->ChangeColorOfQuadVertices(TextCharQuadIndex, CR, CG, CB, CA);
						++TextCharQuadIndex;
					}

					// render non outlined
					Graphics()->QuadsDrawCurrentVertices(false);
				}
				else
					Graphics()->QuadsEnd();

				// reset
				Graphics()->SetColor(1.f, 1.f, 1.f, 1.f);
			}
		}

		if(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex != -1)
		{
			if(TextContainer.m_HasSelection)
			{
				Graphics()->TextureClear();
				Graphics()->SetColor(m_SelectionColor);
				Graphics()->RenderQuadContainerEx(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, TextContainer.m_HasCursor ? 2 : 0, -1, 0, 0);
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			}

			if(TextContainer.m_HasCursor)
			{
				const auto CurTime = time_get_nanoseconds();

				Graphics()->TextureClear();
				if(TextContainer.m_ForceCursorRendering || (CurTime - m_CursorRenderTime) > 500ms)
				{
					Graphics()->SetColor(TextOutlineColor);
					Graphics()->RenderQuadContainerEx(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, 0, 1, 0, 0);
					Graphics()->SetColor(TextColor);
					Graphics()->RenderQuadContainerEx(TextContainer.m_StringInfo.m_SelectionQuadContainerIndex, 1, 1, 0, 0);
				}
				if(TextContainer.m_ForceCursorRendering)
					m_CursorRenderTime = CurTime - 501ms;
				else if((CurTime - m_CursorRenderTime) > 1s)
					m_CursorRenderTime = time_get_nanoseconds();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}
	}

	void RenderTextContainer(STextContainerIndex TextContainerIndex, const ColorRGBA &TextColor, const ColorRGBA &TextOutlineColor, float X, float Y) override
	{
		STextContainer &TextContainer = GetTextContainer(TextContainerIndex);

		// remap the current screen, after render revert the change again
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

		if((TextContainer.m_RenderFlags & TEXT_RENDER_FLAG_NO_PIXEL_ALIGMENT) == 0)
		{
			const vec2 FakeToScreen = vec2(Graphics()->ScreenWidth() / (ScreenX1 - ScreenX0), Graphics()->ScreenHeight() / (ScreenY1 - ScreenY0));
			const float AlignedX = round_to_int((TextContainer.m_X + X) * FakeToScreen.x) / FakeToScreen.x;
			const float AlignedY = round_to_int((TextContainer.m_Y + Y) * FakeToScreen.y) / FakeToScreen.y;
			X = AlignedX - TextContainer.m_AlignedStartX;
			Y = AlignedY - TextContainer.m_AlignedStartY;
		}

		TextContainer.m_BoundingBox.m_X = X;
		TextContainer.m_BoundingBox.m_Y = Y;

		Graphics()->MapScreen(ScreenX0 - X, ScreenY0 - Y, ScreenX1 - X, ScreenY1 - Y);
		RenderTextContainer(TextContainerIndex, TextColor, TextOutlineColor);
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}

	STextBoundingBox GetBoundingBoxTextContainer(STextContainerIndex TextContainerIndex) override
	{
		const STextContainer &TextContainer = GetTextContainer(TextContainerIndex);
		return TextContainer.m_BoundingBox;
	}

	void UploadEntityLayerText(const CImageInfo &TextImage, int TexSubWidth, int TexSubHeight, const char *pText, int Length, float x, float y, int FontSize) override
	{
		m_pGlyphMap->UploadEntityLayerText(TextImage, TexSubWidth, TexSubHeight, pText, Length, x, y, FontSize);
	}

	int AdjustFontSize(const char *pText, int TextLength, int MaxSize, int MaxWidth) const override
	{
		const int WidthOfText = CalculateTextWidth(pText, TextLength, 0, 100);

		int FontSize = 100.0f / ((float)WidthOfText / (float)MaxWidth);
		if(MaxSize > 0 && FontSize > MaxSize)
			FontSize = MaxSize;

		return FontSize;
	}

	float GetGlyphOffsetX(int FontSize, char TextCharacter) const override
	{
		if(m_pGlyphMap->DefaultFace() == nullptr)
			return -1.0f;

		FT_Set_Pixel_Sizes(m_pGlyphMap->DefaultFace(), 0, FontSize);
		const char *pTmp = &TextCharacter;
		const int NextCharacter = str_utf8_decode(&pTmp);

		if(NextCharacter)
		{
#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 7 && (FREETYPE_MINOR > 7 || FREETYPE_PATCH >= 1)
			const FT_Int32 FTFlags = FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_NO_BITMAP;
#else
			const FT_Int32 FTFlags = FT_LOAD_RENDER | FT_LOAD_NO_BITMAP;
#endif
			if(FT_Load_Char(m_pGlyphMap->DefaultFace(), NextCharacter, FTFlags))
			{
				log_debug("textrender", "Error loading glyph. Chr=%d", NextCharacter);
				return -1.0f;
			}

			return (float)(m_pGlyphMap->DefaultFace()->glyph->metrics.horiBearingX >> 6);
		}
		return 0.0f;
	}

	int CalculateTextWidth(const char *pText, int TextLength, int FontWidth, int FontHeight) const override
	{
		if(m_pGlyphMap->DefaultFace() == nullptr)
			return 0;

		const char *pCurrent = pText;
		const char *pEnd = pCurrent + TextLength;

		int WidthOfText = 0;
		FT_Set_Pixel_Sizes(m_pGlyphMap->DefaultFace(), FontWidth, FontHeight);
		while(pCurrent < pEnd)
		{
			const char *pTmp = pCurrent;
			const int NextCharacter = str_utf8_decode(&pTmp);
			if(NextCharacter)
			{
#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 7 && (FREETYPE_MINOR > 7 || FREETYPE_PATCH >= 1)
				const FT_Int32 FTFlags = FT_LOAD_BITMAP_METRICS_ONLY | FT_LOAD_NO_BITMAP;
#else
				const FT_Int32 FTFlags = FT_LOAD_RENDER | FT_LOAD_NO_BITMAP;
#endif
				if(FT_Load_Char(m_pGlyphMap->DefaultFace(), NextCharacter, FTFlags))
				{
					log_debug("textrender", "Error loading glyph. Chr=%d", NextCharacter);
					pCurrent = pTmp;
					continue;
				}

				WidthOfText += (m_pGlyphMap->DefaultFace()->glyph->metrics.width >> 6) + 1;
			}
			pCurrent = pTmp;
		}

		return WidthOfText;
	}

	void OnPreWindowResize() override
	{
		for(auto *pTextContainer : m_vpTextContainers)
		{
			if(pTextContainer->m_ContainerIndex.Valid() && pTextContainer->m_ContainerIndex.m_UseCount.use_count() <= 1)
			{
				log_error("textrender", "Found non empty text container with index %d with %" PRIzu " quads '%s'", pTextContainer->m_StringInfo.m_QuadBufferContainerIndex, pTextContainer->m_StringInfo.m_vCharacterQuads.size(), pTextContainer->m_aDebugText);
				dbg_assert(false, "Text container was forgotten by the implementation (the index was overwritten).");
			}
		}
	}

	void OnWindowResize() override
	{
		bool HasNonEmptyTextContainer = false;
		for(auto *pTextContainer : m_vpTextContainers)
		{
			if(pTextContainer->m_StringInfo.m_QuadBufferContainerIndex != -1)
			{
				log_error("textrender", "Found non empty text container with index %d with %" PRIzu " quads '%s'", pTextContainer->m_StringInfo.m_QuadBufferContainerIndex, pTextContainer->m_StringInfo.m_vCharacterQuads.size(), pTextContainer->m_aDebugText);
				log_error("textrender", "The text container index was in use by %d ", (int)pTextContainer->m_ContainerIndex.m_UseCount.use_count());
				HasNonEmptyTextContainer = true;
			}
		}

		dbg_assert(!HasNonEmptyTextContainer, "text container was not empty");
	}
};

IEngineTextRender *CreateEngineTextRender() { return new CTextRender; }
