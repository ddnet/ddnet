/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CAMERA_H
#define GAME_CLIENT_COMPONENTS_CAMERA_H
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/console.h>

#include <game/bezier.h>
#include <game/client/component.h>

class CCamera : public CComponent
{
	friend class CMenuBackground;

	enum
	{
		CAMTYPE_UNDEFINED = -1,
		CAMTYPE_SPEC,
		CAMTYPE_PLAYER,
	};

	int m_CamType = 0;
	vec2 m_aLastPos[NUM_DUMMIES];
	vec2 m_PrevCenter;

	CCubicBezier m_ZoomSmoothing;
	float m_ZoomSmoothingStart = 0;
	float m_ZoomSmoothingEnd = 0;

	void ScaleZoom(float Factor);
	void ChangeZoom(float Target);
	float ZoomProgress(float CurrentTime) const;

	float MinZoomLevel();
	float MaxZoomLevel();

public:
	vec2 m_Center;
	bool m_ZoomSet = false;
	bool m_Zooming = false;
	float m_Zoom = 0;
	float m_ZoomSmoothingTarget = 0;

	CCamera();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;

	// DDRace

	virtual void OnConsoleInit() override;
	virtual void OnReset() override;

private:
	static void ConZoomPlus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoomMinus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoom(IConsole::IResult *pResult, void *pUserData);
	static void ConSetView(IConsole::IResult *pResult, void *pUserData);

	vec2 m_ForceFreeviewPos;
};

#endif
