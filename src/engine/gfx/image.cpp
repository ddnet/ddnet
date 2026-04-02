#include <base/dbg.h>
#include <base/log.h>
#include <base/mem.h>

#include <engine/image.h>

#include <cstdlib>

CImageInfo::CImageInfo(CImageInfo &&Other)
{
	*this = std::move(Other);
}

CImageInfo &CImageInfo::operator=(CImageInfo &&Other)
{
	m_Width = Other.m_Width;
	m_Height = Other.m_Height;
	m_Format = Other.m_Format;
	m_pData = Other.m_pData;
	m_IsAllocated = Other.m_IsAllocated;
	Other.m_Width = 0;
	Other.m_Height = 0;
	Other.m_Format = FORMAT_UNDEFINED;
	Other.m_pData = nullptr;
	Other.m_IsAllocated = false;
	return *this;
}

CImageInfo::~CImageInfo()
{
	if(m_IsAllocated)
		Free();
}

void CImageInfo::Allocate()
{
	dbg_assert(m_pData == nullptr && !m_IsAllocated, "Image data already allocated");
	// format is asserted in pixel size
	m_pData = static_cast<uint8_t *>(malloc(DataSize()));
	m_IsAllocated = true;
}

void CImageInfo::AllocateFillZero()
{
	dbg_assert(m_pData == nullptr && !m_IsAllocated, "Image data already allocated");
	// format is asserted in pixel size
	m_pData = static_cast<uint8_t *>(calloc(DataSize(), sizeof(uint8_t)));
	m_IsAllocated = true;
}

void CImageInfo::Free()
{
	if(m_pData != nullptr)
	{
		if(!m_IsAllocated)
			log_warn("graphics", "Image free without calling 'Allocate'");
		free(m_pData);
		m_pData = nullptr;
	}
	m_IsAllocated = false;
	m_Width = 0;
	m_Height = 0;
	m_Format = FORMAT_UNDEFINED;
}

size_t CImageInfo::PixelSize(EImageFormat Format)
{
	dbg_assert(Format != FORMAT_UNDEFINED, "Format undefined");
	static const size_t s_aSizes[] = {3, 4, 1, 2};
	return s_aSizes[(int)Format];
}

const char *CImageInfo::FormatName(EImageFormat Format)
{
	static const char *s_apNames[] = {"UNDEFINED", "RGB", "RGBA", "R", "RA"};
	return s_apNames[(int)Format + 1];
}

size_t CImageInfo::PixelSize() const
{
	return PixelSize(m_Format);
}

const char *CImageInfo::FormatName() const
{
	return FormatName(m_Format);
}

size_t CImageInfo::DataSize() const
{
	return m_Width * m_Height * PixelSize(m_Format);
}

bool CImageInfo::DataEquals(const CImageInfo &Other) const
{
	if(m_Width != Other.m_Width || m_Height != Other.m_Height || m_Format != Other.m_Format)
		return false;
	if(m_pData == nullptr && Other.m_pData == nullptr)
		return true;
	if(m_pData == nullptr || Other.m_pData == nullptr)
		return false;
	return mem_comp(m_pData, Other.m_pData, DataSize()) == 0;
}

ColorRGBA CImageInfo::PixelColor(size_t x, size_t y) const
{
	const size_t PixelStartIndex = (x + m_Width * y) * PixelSize();

	ColorRGBA Color;
	if(m_Format == FORMAT_R)
	{
		Color.r = Color.g = Color.b = 1.0f;
		Color.a = m_pData[PixelStartIndex] / 255.0f;
	}
	else if(m_Format == FORMAT_RA)
	{
		Color.r = Color.g = Color.b = m_pData[PixelStartIndex] / 255.0f;
		Color.a = m_pData[PixelStartIndex + 1] / 255.0f;
	}
	else
	{
		Color.r = m_pData[PixelStartIndex + 0] / 255.0f;
		Color.g = m_pData[PixelStartIndex + 1] / 255.0f;
		Color.b = m_pData[PixelStartIndex + 2] / 255.0f;
		if(m_Format == FORMAT_RGBA)
		{
			Color.a = m_pData[PixelStartIndex + 3] / 255.0f;
		}
		else
		{
			Color.a = 1.0f;
		}
	}
	return Color;
}

void CImageInfo::SetPixelColor(size_t x, size_t y, ColorRGBA Color) const
{
	const size_t PixelStartIndex = (x + m_Width * y) * PixelSize();

	if(m_Format == FORMAT_R)
	{
		m_pData[PixelStartIndex] = round_to_int(Color.a * 255.0f);
	}
	else if(m_Format == FORMAT_RA)
	{
		m_pData[PixelStartIndex] = round_to_int((Color.r + Color.g + Color.b) / 3.0f * 255.0f);
		m_pData[PixelStartIndex + 1] = round_to_int(Color.a * 255.0f);
	}
	else
	{
		m_pData[PixelStartIndex + 0] = round_to_int(Color.r * 255.0f);
		m_pData[PixelStartIndex + 1] = round_to_int(Color.g * 255.0f);
		m_pData[PixelStartIndex + 2] = round_to_int(Color.b * 255.0f);
		if(m_Format == FORMAT_RGBA)
		{
			m_pData[PixelStartIndex + 3] = round_to_int(Color.a * 255.0f);
		}
	}
}

void CImageInfo::CopyRectFrom(const CImageInfo &SrcImage, size_t SrcX, size_t SrcY, size_t Width, size_t Height, size_t DestX, size_t DestY) const
{
	const size_t PixelSize = SrcImage.PixelSize();
	const size_t CopySize = Width * PixelSize;
	for(size_t Y = 0; Y < Height; ++Y)
	{
		const size_t SrcOffset = ((SrcY + Y) * SrcImage.m_Width + SrcX) * PixelSize;
		const size_t DestOffset = ((DestY + Y) * m_Width + DestX) * PixelSize;
		mem_copy(&m_pData[DestOffset], &SrcImage.m_pData[SrcOffset], CopySize);
	}
}

CImageInfo CImageInfo::DeepCopy() const
{
	const size_t Size = DataSize();

	CImageInfo Copy;
	Copy.m_Width = m_Width;
	Copy.m_Height = m_Height;
	Copy.m_Format = m_Format;
	Copy.Allocate();
	mem_copy(Copy.m_pData, m_pData, Size);
	return Copy;
}
