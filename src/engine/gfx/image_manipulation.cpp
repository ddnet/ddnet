#include "image_manipulation.h"

#include <base/math.h>
#include <base/system.h>

bool ConvertToRgba(uint8_t *pDest, const CImageInfo &SourceImage)
{
	if(SourceImage.m_Format == CImageInfo::FORMAT_RGBA)
	{
		mem_copy(pDest, SourceImage.m_pData, SourceImage.DataSize());
		return true;
	}
	else
	{
		const size_t SrcChannelCount = CImageInfo::PixelSize(SourceImage.m_Format);
		const size_t DstChannelCount = CImageInfo::PixelSize(CImageInfo::FORMAT_RGBA);
		for(size_t Y = 0; Y < SourceImage.m_Height; ++Y)
		{
			for(size_t X = 0; X < SourceImage.m_Width; ++X)
			{
				size_t ImgOffsetSrc = (Y * SourceImage.m_Width * SrcChannelCount) + (X * SrcChannelCount);
				size_t ImgOffsetDest = (Y * SourceImage.m_Width * DstChannelCount) + (X * DstChannelCount);
				if(SourceImage.m_Format == CImageInfo::FORMAT_RGB)
				{
					mem_copy(&pDest[ImgOffsetDest], &SourceImage.m_pData[ImgOffsetSrc], SrcChannelCount);
					pDest[ImgOffsetDest + 3] = 255;
				}
				else if(SourceImage.m_Format == CImageInfo::FORMAT_RA)
				{
					pDest[ImgOffsetDest + 0] = SourceImage.m_pData[ImgOffsetSrc];
					pDest[ImgOffsetDest + 1] = SourceImage.m_pData[ImgOffsetSrc];
					pDest[ImgOffsetDest + 2] = SourceImage.m_pData[ImgOffsetSrc];
					pDest[ImgOffsetDest + 3] = SourceImage.m_pData[ImgOffsetSrc + 1];
				}
				else if(SourceImage.m_Format == CImageInfo::FORMAT_R)
				{
					pDest[ImgOffsetDest + 0] = 255;
					pDest[ImgOffsetDest + 1] = 255;
					pDest[ImgOffsetDest + 2] = 255;
					pDest[ImgOffsetDest + 3] = SourceImage.m_pData[ImgOffsetSrc];
				}
				else
				{
					dbg_assert(false, "SourceImage.m_Format invalid");
				}
			}
		}
		return false;
	}
}

bool ConvertToRgbaAlloc(uint8_t *&pDest, const CImageInfo &SourceImage)
{
	pDest = static_cast<uint8_t *>(malloc(SourceImage.m_Width * SourceImage.m_Height * CImageInfo::PixelSize(CImageInfo::FORMAT_RGBA)));
	return ConvertToRgba(pDest, SourceImage);
}

bool ConvertToRgba(CImageInfo &Image)
{
	if(Image.m_Format == CImageInfo::FORMAT_RGBA)
		return true;

	uint8_t *pRgbaData;
	ConvertToRgbaAlloc(pRgbaData, Image);
	free(Image.m_pData);
	Image.m_pData = pRgbaData;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	return false;
}

void ConvertToGrayscale(const CImageInfo &Image)
{
	if(Image.m_Format == CImageInfo::FORMAT_R || Image.m_Format == CImageInfo::FORMAT_RA)
		return;

	const size_t Step = Image.PixelSize();
	for(size_t i = 0; i < Image.m_Width * Image.m_Height; ++i)
	{
		const int Average = (Image.m_pData[i * Step] + Image.m_pData[i * Step + 1] + Image.m_pData[i * Step + 2]) / 3;
		Image.m_pData[i * Step] = Average;
		Image.m_pData[i * Step + 1] = Average;
		Image.m_pData[i * Step + 2] = Average;
	}
}

static constexpr int DILATE_BPP = 4; // RGBA assumed
static constexpr uint8_t DILATE_ALPHA_THRESHOLD = 10;

static void Dilate(int w, int h, const uint8_t *pSrc, uint8_t *pDest)
{
	const int aDirX[] = {0, -1, 1, 0};
	const int aDirY[] = {-1, 0, 0, 1};

	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += DILATE_BPP)
		{
			for(int i = 0; i < DILATE_BPP; ++i)
				pDest[m + i] = pSrc[m + i];
			if(pSrc[m + DILATE_BPP - 1] > DILATE_ALPHA_THRESHOLD)
				continue;

			// --- Implementation Note ---
			// The sum and counter variable can be used to compute a smoother dilated image.
			// In this reference implementation, the loop breaks as soon as Counter == 1.
			// We break the loop here to match the selection of the previously used algorithm.
			int aSumOfOpaque[] = {0, 0, 0};
			int Counter = 0;
			for(int c = 0; c < 4; c++)
			{
				const int ClampedX = clamp(x + aDirX[c], 0, w - 1);
				const int ClampedY = clamp(y + aDirY[c], 0, h - 1);
				const int SrcIndex = ClampedY * w * DILATE_BPP + ClampedX * DILATE_BPP;
				if(pSrc[SrcIndex + DILATE_BPP - 1] > DILATE_ALPHA_THRESHOLD)
				{
					for(int p = 0; p < DILATE_BPP - 1; ++p)
						aSumOfOpaque[p] += pSrc[SrcIndex + p];
					++Counter;
					break;
				}
			}

			if(Counter > 0)
			{
				for(int i = 0; i < DILATE_BPP - 1; ++i)
				{
					aSumOfOpaque[i] /= Counter;
					pDest[m + i] = (uint8_t)aSumOfOpaque[i];
				}

				pDest[m + DILATE_BPP - 1] = 255;
			}
		}
	}
}

static void CopyColorValues(int w, int h, const uint8_t *pSrc, uint8_t *pDest)
{
	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += DILATE_BPP)
		{
			if(pDest[m + DILATE_BPP - 1] == 0)
			{
				mem_copy(&pDest[m], &pSrc[m], DILATE_BPP - 1);
			}
		}
	}
}

void DilateImage(uint8_t *pImageBuff, int w, int h)
{
	DilateImageSub(pImageBuff, w, h, 0, 0, w, h);
}

void DilateImage(const CImageInfo &Image)
{
	dbg_assert(Image.m_Format == CImageInfo::FORMAT_RGBA, "Dilate requires RGBA format");
	DilateImage(Image.m_pData, Image.m_Width, Image.m_Height);
}

void DilateImageSub(uint8_t *pImageBuff, int w, int h, int x, int y, int SubWidth, int SubHeight)
{
	uint8_t *apBuffer[2] = {nullptr, nullptr};

	const size_t ImageSize = (size_t)SubWidth * SubHeight * sizeof(uint8_t) * DILATE_BPP;
	apBuffer[0] = (uint8_t *)malloc(ImageSize);
	apBuffer[1] = (uint8_t *)malloc(ImageSize);
	uint8_t *pBufferOriginal = (uint8_t *)malloc(ImageSize);

	for(int Y = 0; Y < SubHeight; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * DILATE_BPP) + (x * DILATE_BPP);
		int DstImgOffset = (Y * SubWidth * DILATE_BPP);
		int CopySize = SubWidth * DILATE_BPP;
		mem_copy(&pBufferOriginal[DstImgOffset], &pImageBuff[SrcImgOffset], CopySize);
	}

	Dilate(SubWidth, SubHeight, pBufferOriginal, apBuffer[0]);

	for(int i = 0; i < 5; i++)
	{
		Dilate(SubWidth, SubHeight, apBuffer[0], apBuffer[1]);
		Dilate(SubWidth, SubHeight, apBuffer[1], apBuffer[0]);
	}

	CopyColorValues(SubWidth, SubHeight, apBuffer[0], pBufferOriginal);

	free(apBuffer[0]);
	free(apBuffer[1]);

	for(int Y = 0; Y < SubHeight; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * DILATE_BPP) + (x * DILATE_BPP);
		int DstImgOffset = (Y * SubWidth * DILATE_BPP);
		int CopySize = SubWidth * DILATE_BPP;
		mem_copy(&pImageBuff[SrcImgOffset], &pBufferOriginal[DstImgOffset], CopySize);
	}

	free(pBufferOriginal);
}

static float CubicHermite(float A, float B, float C, float D, float t)
{
	float a = -A / 2.0f + (3.0f * B) / 2.0f - (3.0f * C) / 2.0f + D / 2.0f;
	float b = A - (5.0f * B) / 2.0f + 2.0f * C - D / 2.0f;
	float c = -A / 2.0f + C / 2.0f;
	float d = B;

	return (a * t * t * t) + (b * t * t) + (c * t) + d;
}

static void GetPixelClamped(const uint8_t *pSourceImage, int x, int y, uint32_t W, uint32_t H, size_t BPP, uint8_t aSample[4])
{
	x = clamp<int>(x, 0, (int)W - 1);
	y = clamp<int>(y, 0, (int)H - 1);

	mem_copy(aSample, &pSourceImage[x * BPP + (W * BPP * y)], BPP);
}

static void SampleBicubic(const uint8_t *pSourceImage, float u, float v, uint32_t W, uint32_t H, size_t BPP, uint8_t aSample[4])
{
	float X = (u * W) - 0.5f;
	int xInt = (int)X;
	float xFract = X - std::floor(X);

	float Y = (v * H) - 0.5f;
	int yInt = (int)Y;
	float yFract = Y - std::floor(Y);

	uint8_t aaaSamples[4][4][4];
	for(int y = 0; y < 4; ++y)
	{
		for(int x = 0; x < 4; ++x)
		{
			GetPixelClamped(pSourceImage, xInt + x - 1, yInt + y - 1, W, H, BPP, aaaSamples[x][y]);
		}
	}

	for(size_t i = 0; i < BPP; i++)
	{
		float aRows[4];
		for(int y = 0; y < 4; ++y)
		{
			aRows[y] = CubicHermite(aaaSamples[0][y][i], aaaSamples[1][y][i], aaaSamples[2][y][i], aaaSamples[3][y][i], xFract);
		}
		aSample[i] = (uint8_t)clamp<float>(CubicHermite(aRows[0], aRows[1], aRows[2], aRows[3], yFract), 0.0f, 255.0f);
	}
}

static void ResizeImage(const uint8_t *pSourceImage, uint32_t SW, uint32_t SH, uint8_t *pDestinationImage, uint32_t W, uint32_t H, size_t BPP)
{
	for(int y = 0; y < (int)H; ++y)
	{
		float v = (float)y / (float)(H - 1);
		for(int x = 0; x < (int)W; ++x)
		{
			float u = (float)x / (float)(W - 1);
			uint8_t aSample[4];
			SampleBicubic(pSourceImage, u, v, SW, SH, BPP, aSample);
			mem_copy(&pDestinationImage[x * BPP + ((W * BPP) * y)], aSample, BPP);
		}
	}
}

uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP)
{
	uint8_t *pTmpData = (uint8_t *)malloc((size_t)NewWidth * NewHeight * BPP);
	ResizeImage(pImageData, Width, Height, pTmpData, NewWidth, NewHeight, BPP);
	return pTmpData;
}

void ResizeImage(CImageInfo &Image, int NewWidth, int NewHeight)
{
	uint8_t *pNewData = ResizeImage(Image.m_pData, Image.m_Width, Image.m_Height, NewWidth, NewHeight, Image.PixelSize());
	free(Image.m_pData);
	Image.m_pData = pNewData;
	Image.m_Width = NewWidth;
	Image.m_Height = NewHeight;
}

int HighestBit(int OfVar)
{
	if(!OfVar)
		return 0;

	int RetV = 1;

	while(OfVar >>= 1)
		RetV <<= 1;

	return RetV;
}
