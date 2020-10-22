#include <base/math.h>
#include <base/system.h>

static void Dilate(int w, int h, int BPP, unsigned char *pSrc, unsigned char *pDest, unsigned char AlphaThreshold = 30)
{
	int ix, iy;
	const int xo[] = {0, -1, 1, 0};
	const int yo[] = {-1, 0, 0, 1};

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

			int SumOfOpaque[] = {0, 0, 0};
			int Counter = 0;
			for(int c = 0; c < 4; c++)
			{
				ix = clamp(x + xo[c], 0, w - 1);
				iy = clamp(y + yo[c], 0, h - 1);
				int k = iy * w * BPP + ix * BPP;
				if(pSrc[k + AlphaCompIndex] > AlphaThreshold)
				{
					for(int p = 0; p < BPP - 1; ++p)
						// Seems safe for BPP = 3, 4 which we use. clang-analyzer seems to
						// asssume being called with larger value. TODO: Can make this
						// safer anyway.
						SumOfOpaque[p] += pSrc[k + p]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
					++Counter;
					break;
				}
			}

			if(Counter > 0)
			{
				for(int i = 0; i < BPP - 1; ++i)
				{
					SumOfOpaque[i] /= Counter;
					pDest[m + i] = (unsigned char)SumOfOpaque[i];
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
	unsigned char *pBuffer[2] = {NULL, NULL};

	pBuffer[0] = (unsigned char *)malloc((size_t)w * h * sizeof(unsigned char) * BPP);
	pBuffer[1] = (unsigned char *)malloc((size_t)w * h * sizeof(unsigned char) * BPP);

	unsigned char *pPixelBuff = (unsigned char *)pImageBuff;

	Dilate(w, h, BPP, pPixelBuff, pBuffer[0]);

	for(int i = 0; i < 5; i++)
	{
		Dilate(w, h, BPP, pBuffer[0], pBuffer[1]);
		Dilate(w, h, BPP, pBuffer[1], pBuffer[0]);
	}

	CopyColorValues(w, h, BPP, pBuffer[0], pPixelBuff);

	free(pBuffer[0]);
	free(pBuffer[1]);
}
