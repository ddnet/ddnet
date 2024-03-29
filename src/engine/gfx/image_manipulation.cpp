#include "image_manipulation.h"
#include <base/math.h>
#include <base/system.h>

#define TW_DILATE_ALPHA_THRESHOLD 10

static void Dilate(int w, int h, const uint8_t *pSrc, uint8_t *pDest, uint8_t AlphaThreshold = TW_DILATE_ALPHA_THRESHOLD)
{
	const int BPP = 4; // RGBA assumed
	int ix, iy;
	const int aDirX[] = {0, -1, 1, 0};
	const int aDirY[] = {-1, 0, 0, 1};

	int AlphaCompIndex = BPP - 1;

	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += BPP)
		{
			for(int i = 0; i < BPP; ++i)
				pDest[m + i] = pSrc[m + i];
			if(pSrc[m + AlphaCompIndex] > AlphaThreshold)
				continue;

			int aSumOfOpaque[] = {0, 0, 0};
			int Counter = 0;
			for(int c = 0; c < 4; c++)
			{
				ix = clamp(x + aDirX[c], 0, w - 1);
				iy = clamp(y + aDirY[c], 0, h - 1);
				int k = iy * w * BPP + ix * BPP;
				if(pSrc[k + AlphaCompIndex] > AlphaThreshold)
				{
					for(int p = 0; p < BPP - 1; ++p)
						aSumOfOpaque[p] += pSrc[k + p];
					++Counter;
					break;
				}
			}

			if(Counter > 0)
			{
				for(int i = 0; i < BPP - 1; ++i)
				{
					aSumOfOpaque[i] /= Counter;
					pDest[m + i] = (uint8_t)aSumOfOpaque[i];
				}

				pDest[m + AlphaCompIndex] = 255;
			}
		}
	}
}

static void CopyColorValues(int w, int h, int BPP, const uint8_t *pSrc, uint8_t *pDest)
{
	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += BPP)
		{
			for(int i = 0; i < BPP - 1; ++i)
			{
				if(pDest[m + 3] == 0)
					pDest[m + i] = pSrc[m + i];
			}
		}
	}
}

void DilateImage(uint8_t *pImageBuff, int w, int h)
{
	DilateImageSub(pImageBuff, w, h, 0, 0, w, h);
}

void DilateImageSub(uint8_t *pImageBuff, int w, int h, int x, int y, int sw, int sh)
{
	const int BPP = 4; // RGBA assumed
	uint8_t *apBuffer[2] = {NULL, NULL};

	apBuffer[0] = (uint8_t *)malloc((size_t)sw * sh * sizeof(uint8_t) * BPP);
	apBuffer[1] = (uint8_t *)malloc((size_t)sw * sh * sizeof(uint8_t) * BPP);
	uint8_t *pBufferOriginal = (uint8_t *)malloc((size_t)sw * sh * sizeof(uint8_t) * BPP);

	for(int Y = 0; Y < sh; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * BPP) + (x * BPP);
		int DstImgOffset = (Y * sw * BPP);
		int CopySize = sw * BPP;
		mem_copy(&pBufferOriginal[DstImgOffset], &pImageBuff[SrcImgOffset], CopySize);
	}

	Dilate(sw, sh, pBufferOriginal, apBuffer[0]);

	for(int i = 0; i < 5; i++)
	{
		Dilate(sw, sh, apBuffer[0], apBuffer[1]);
		Dilate(sw, sh, apBuffer[1], apBuffer[0]);
	}

	CopyColorValues(sw, sh, BPP, apBuffer[0], pBufferOriginal);

	free(apBuffer[0]);
	free(apBuffer[1]);

	for(int Y = 0; Y < sh; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * BPP) + (x * BPP);
		int DstImgOffset = (Y * sw * BPP);
		int CopySize = sw * BPP;
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

static void GetPixelClamped(const uint8_t *pSourceImage, int x, int y, uint32_t W, uint32_t H, size_t BPP, uint8_t aTmp[])
{
	x = clamp<int>(x, 0, (int)W - 1);
	y = clamp<int>(y, 0, (int)H - 1);

	for(size_t i = 0; i < BPP; i++)
	{
		aTmp[i] = pSourceImage[x * BPP + (W * BPP * y) + i];
	}
}

static void SampleBicubic(const uint8_t *pSourceImage, float u, float v, uint32_t W, uint32_t H, size_t BPP, uint8_t aSample[])
{
	float X = (u * W) - 0.5f;
	int xInt = (int)X;
	float xFract = X - std::floor(X);

	float Y = (v * H) - 0.5f;
	int yInt = (int)Y;
	float yFract = Y - std::floor(Y);

	uint8_t aPX00[4];
	uint8_t aPX10[4];
	uint8_t aPX20[4];
	uint8_t aPX30[4];

	uint8_t aPX01[4];
	uint8_t aPX11[4];
	uint8_t aPX21[4];
	uint8_t aPX31[4];

	uint8_t aPX02[4];
	uint8_t aPX12[4];
	uint8_t aPX22[4];
	uint8_t aPX32[4];

	uint8_t aPX03[4];
	uint8_t aPX13[4];
	uint8_t aPX23[4];
	uint8_t aPX33[4];

	GetPixelClamped(pSourceImage, xInt - 1, yInt - 1, W, H, BPP, aPX00);
	GetPixelClamped(pSourceImage, xInt + 0, yInt - 1, W, H, BPP, aPX10);
	GetPixelClamped(pSourceImage, xInt + 1, yInt - 1, W, H, BPP, aPX20);
	GetPixelClamped(pSourceImage, xInt + 2, yInt - 1, W, H, BPP, aPX30);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 0, W, H, BPP, aPX01);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 0, W, H, BPP, aPX11);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 0, W, H, BPP, aPX21);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 0, W, H, BPP, aPX31);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 1, W, H, BPP, aPX02);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 1, W, H, BPP, aPX12);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 1, W, H, BPP, aPX22);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 1, W, H, BPP, aPX32);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 2, W, H, BPP, aPX03);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 2, W, H, BPP, aPX13);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 2, W, H, BPP, aPX23);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 2, W, H, BPP, aPX33);

	for(size_t i = 0; i < BPP; i++)
	{
		float Clmn0 = CubicHermite(aPX00[i], aPX10[i], aPX20[i], aPX30[i], xFract);
		float Clmn1 = CubicHermite(aPX01[i], aPX11[i], aPX21[i], aPX31[i], xFract);
		float Clmn2 = CubicHermite(aPX02[i], aPX12[i], aPX22[i], aPX32[i], xFract);
		float Clmn3 = CubicHermite(aPX03[i], aPX13[i], aPX23[i], aPX33[i], xFract);

		float Valuef = CubicHermite(Clmn0, Clmn1, Clmn2, Clmn3, yFract);

		Valuef = clamp<float>(Valuef, 0.0f, 255.0f);

		aSample[i] = (uint8_t)Valuef;
	}
}

static void ResizeImage(const uint8_t *pSourceImage, uint32_t SW, uint32_t SH, uint8_t *pDestinationImage, uint32_t W, uint32_t H, size_t BPP)
{
	uint8_t aSample[4];
	int y, x;

	for(y = 0; y < (int)H; ++y)
	{
		float v = (float)y / (float)(H - 1);

		for(x = 0; x < (int)W; ++x)
		{
			float u = (float)x / (float)(W - 1);
			SampleBicubic(pSourceImage, u, v, SW, SH, BPP, aSample);

			for(size_t i = 0; i < BPP; ++i)
			{
				pDestinationImage[x * BPP + ((W * BPP) * y) + i] = aSample[i];
			}
		}
	}
}

uint8_t *ResizeImage(const uint8_t *pImageData, int Width, int Height, int NewWidth, int NewHeight, int BPP)
{
	uint8_t *pTmpData = (uint8_t *)malloc((size_t)NewWidth * NewHeight * BPP);
	ResizeImage(pImageData, Width, Height, pTmpData, NewWidth, NewHeight, BPP);
	return pTmpData;
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
