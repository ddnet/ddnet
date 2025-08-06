#include "graphics.h"

// helper functions
void IGraphics::CalcScreenParams(float Aspect, float Zoom, float *pWidth, float *pHeight)
{
	const float Amount = 1150 * 1000;
	const float WMax = 1500;
	const float HMax = 1050;

	const float f = std::sqrt(Amount) / std::sqrt(Aspect);
	*pWidth = f * Aspect;
	*pHeight = f;

	// limit the view
	if(*pWidth > WMax)
	{
		*pWidth = WMax;
		*pHeight = *pWidth / Aspect;
	}

	if(*pHeight > HMax)
	{
		*pHeight = HMax;
		*pWidth = *pHeight * Aspect;
	}

	*pWidth *= Zoom;
	*pHeight *= Zoom;
}

void IGraphics::MapScreenToWorld(float CenterX, float CenterY, float ParallaxX, float ParallaxY,
	float ParallaxZoom, float OffsetX, float OffsetY, float Aspect, float Zoom, float *pPoints)
{
	float Width, Height;
	CalcScreenParams(Aspect, Zoom, &Width, &Height);

	float Scale = (ParallaxZoom * (Zoom - 1.0f) + 100.0f) / 100.0f / Zoom;
	Width *= Scale;
	Height *= Scale;

	CenterX *= ParallaxX / 100.0f;
	CenterY *= ParallaxY / 100.0f;
	pPoints[0] = OffsetX + CenterX - Width / 2;
	pPoints[1] = OffsetY + CenterY - Height / 2;
	pPoints[2] = pPoints[0] + Width;
	pPoints[3] = pPoints[1] + Height;
}

void IGraphics::MapScreenToInterface(float CenterX, float CenterY, float Zoom)
{
	float aPoints[4];
	MapScreenToWorld(CenterX, CenterY, 100.0f, 100.0f, 100.0f,
		0, 0, ScreenAspect(), Zoom, aPoints);
	MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
}
