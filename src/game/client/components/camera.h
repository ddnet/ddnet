/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CAMERA_H
#define GAME_CLIENT_COMPONENTS_CAMERA_H
#include <base/vmath.h>
#include <game/bezier.h>
#include <game/client/component.h>

class CCamera : public CComponent
{
	enum
	{
		POS_START = 0,
		NUM_POS,

		CAMTYPE_UNDEFINED=-1,
		CAMTYPE_SPEC,
		CAMTYPE_PLAYER,
	};

	int m_CamType;
	vec2 m_LastPos[2];
	vec2 m_PrevCenter;
	vec2 m_MenuCenter;
	vec2 m_RotationCenter;
	vec2 m_Positions[NUM_POS];
	int m_CurrentPosition;
	vec2 m_AnimationStartPos;
	float m_MoveTime;

	bool m_Zooming;
	CCubicBezier m_ZoomSmoothing;
	float m_ZoomSmoothingStart;
	float m_ZoomSmoothingEnd;

	void ScaleZoom(float Factor);
	void ChangeZoom(float Target);
	float ZoomProgress(float CurrentTime) const;

public:
	vec2 m_Center;
	bool m_ZoomSet;
	float m_Zoom;
	float m_ZoomSmoothingTarget;

	CCamera();
	virtual void OnRender();
	void ChangePosition(int PositionNumber);

	// DDRace

	virtual void OnConsoleInit();
	virtual void OnReset();

private:
	static void ConZoomPlus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoomMinus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoomReset(IConsole::IResult *pResult, void *pUserData);
};

#endif
