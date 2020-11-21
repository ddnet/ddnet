#include "image_manipulation.h"
#include <base/math.h>
#include <base/system.h>

static void Dilate(int w, int h, int BPP, unsigned char *pSrc, unsigned char *pDest, unsigned char AlphaThreshold = TW_DILATE_ALPHA_THRESHOLD)
{
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
						// Seems safe for BPP = 3, 4 which we use. clang-analyzer seems to
						// asssume being called with larger value. TODO: Can make this
						// safer anyway.
						aSumOfOpaque[p] += pSrc[k + p]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
					++Counter;
					break;
				}
			}

			if(Counter > 0)
			{
				for(int i = 0; i < BPP - 1; ++i)
				{
					aSumOfOpaque[i] /= Counter;
					pDest[m + i] = (unsigned char)aSumOfOpaque[i];
				}

				pDest[m + AlphaCompIndex] = 255;
			}
		}
	}
}

static void CopyColorValues(int w, int h, int BPP, unsigned char *pSrc, unsigned char *pDest)
{
	int m = 0;
	for(int y = 0; y < h; y++)
	{
		for(int x = 0; x < w; x++, m += BPP)
		{
			for(int i = 0; i < BPP - 1; ++i)
				pDest[m + i] = pSrc[m + i];
		}
	}
}

void DilateImage(unsigned char *pImageBuff, int w, int h, int BPP)
{
	DilateImageSub(pImageBuff, w, h, BPP, 0, 0, w, h);
}

void DilateImageSub(unsigned char *pImageBuff, int w, int h, int BPP, int x, int y, int sw, int sh)
{
	unsigned char *apBuffer[2] = {NULL, NULL};

	apBuffer[0] = (unsigned char *)malloc((size_t)sw * sh * sizeof(unsigned char) * BPP);
	apBuffer[1] = (unsigned char *)malloc((size_t)sw * sh * sizeof(unsigned char) * BPP);
	unsigned char *pBufferOriginal = (unsigned char *)malloc((size_t)sw * sh * sizeof(unsigned char) * BPP);

	unsigned char *pPixelBuff = (unsigned char *)pImageBuff;

	for(int Y = 0; Y < sh; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * BPP) + (x * BPP);
		int DstImgOffset = (Y * sw * BPP);
		int CopySize = sw * BPP;
		mem_copy(&pBufferOriginal[DstImgOffset], &pPixelBuff[SrcImgOffset], CopySize);
	}

	Dilate(sw, sh, BPP, pBufferOriginal, apBuffer[0]);

	for(int i = 0; i < 5; i++)
	{
		Dilate(sw, sh, BPP, apBuffer[0], apBuffer[1]);
		Dilate(sw, sh, BPP, apBuffer[1], apBuffer[0]);
	}

	CopyColorValues(sw, sh, BPP, apBuffer[0], pBufferOriginal);

	free(apBuffer[0]);
	free(apBuffer[1]);

	for(int Y = 0; Y < sh; ++Y)
	{
		int SrcImgOffset = ((y + Y) * w * BPP) + (x * BPP);
		int DstImgOffset = (Y * sw * BPP);
		int CopySize = sw * BPP;
		mem_copy(&pPixelBuff[SrcImgOffset], &pBufferOriginal[DstImgOffset], CopySize);
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
	float xFract = X - floorf(X);

	float Y = (v * H) - 0.5f;
	int yInt = (int)Y;
	float yFract = Y - floorf(Y);

	uint8_t PX00[4];
	uint8_t PX10[4];
	uint8_t PX20[4];
	uint8_t PX30[4];

	uint8_t PX01[4];
	uint8_t PX11[4];
	uint8_t PX21[4];
	uint8_t PX31[4];

	uint8_t PX02[4];
	uint8_t PX12[4];
	uint8_t PX22[4];
	uint8_t PX32[4];

	uint8_t PX03[4];
	uint8_t PX13[4];
	uint8_t PX23[4];
	uint8_t PX33[4];

	GetPixelClamped(pSourceImage, xInt - 1, yInt - 1, W, H, BPP, PX00);
	GetPixelClamped(pSourceImage, xInt + 0, yInt - 1, W, H, BPP, PX10);
	GetPixelClamped(pSourceImage, xInt + 1, yInt - 1, W, H, BPP, PX20);
	GetPixelClamped(pSourceImage, xInt + 2, yInt - 1, W, H, BPP, PX30);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 0, W, H, BPP, PX01);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 0, W, H, BPP, PX11);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 0, W, H, BPP, PX21);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 0, W, H, BPP, PX31);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 1, W, H, BPP, PX02);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 1, W, H, BPP, PX12);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 1, W, H, BPP, PX22);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 1, W, H, BPP, PX32);

	GetPixelClamped(pSourceImage, xInt - 1, yInt + 2, W, H, BPP, PX03);
	GetPixelClamped(pSourceImage, xInt + 0, yInt + 2, W, H, BPP, PX13);
	GetPixelClamped(pSourceImage, xInt + 1, yInt + 2, W, H, BPP, PX23);
	GetPixelClamped(pSourceImage, xInt + 2, yInt + 2, W, H, BPP, PX33);

	for(size_t i = 0; i < BPP; i++)
	{
		float Clmn0 = CubicHermite(PX00[i], PX10[i], PX20[i], PX30[i], xFract);
		float Clmn1 = CubicHermite(PX01[i], PX11[i], PX21[i], PX31[i], xFract);
		float Clmn2 = CubicHermite(PX02[i], PX12[i], PX22[i], PX32[i], xFract);
		float Clmn3 = CubicHermite(PX03[i], PX13[i], PX23[i], PX33[i], xFract);

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
	// All calls to Resize() ensure width & height are > 0, BPP is always > 0,
	// thus no allocation size 0 possible.
	uint8_t *pTmpData = (uint8_t *)malloc((size_t)NewWidth * NewHeight * BPP); // NOLINT(clang-analyzer-optin.portability.UnixAPI)

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
