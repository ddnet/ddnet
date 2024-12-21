/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CAMERA_H
#define GAME_CLIENT_COMPONENTS_CAMERA_H
#include <base/bezier.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/console.h>

#include <game/client/component.h>

class CCamera : public CComponent
{
	friend class CMenuBackground;

public:
	enum
	{
		CAMTYPE_UNDEFINED = -1,
		CAMTYPE_SPEC,
		CAMTYPE_PLAYER,
	};

private:
	int m_CamType;
	vec2 m_aLastPos[NUM_DUMMIES];
	vec2 m_PrevCenter;

	int m_PrevSpecId;
	bool m_WasSpectating;

	bool m_CameraSmoothing;
	vec2 m_CameraSmoothingCenter;
	vec2 m_CameraSmoothingTarget;
	CCubicBezier m_CameraSmoothingBezierX;
	CCubicBezier m_CameraSmoothingBezierY;
	float m_CameraSmoothingStart;
	float m_CameraSmoothingEnd;
	vec2 m_CenterBeforeSmoothing;

	float CameraSmoothingProgress(float CurrentTime) const;

	CCubicBezier m_ZoomSmoothing;
	float m_ZoomSmoothingStart;
	float m_ZoomSmoothingEnd;

	void ScaleZoom(float Factor);
	void ChangeZoom(float Target, int Smoothness, bool IsUser);
	float ZoomProgress(float CurrentTime) const;

	float MinZoomLevel();
	float MaxZoomLevel();

	vec2 m_LastTargetPos;
	float m_DyncamSmoothingSpeedBias;
	bool m_IsSpectatingPlayer;
	bool m_UsingAutoSpecCamera;

public:
	static constexpr float ZOOM_STEP = 0.866025f;

	/** 
	 * Convert zoom steps to zoom value
	 * 
	 * @param Steps - Zoom steps, 0.0f converts to default zoom (returns 1.0f)
	 * @return converted zoom value
	 **/
	static inline float ZoomStepsToValue(float Steps) { return std::pow(CCamera::ZOOM_STEP, Steps); }

	vec2 m_Center;
	bool m_ZoomSet;
	bool m_Zooming;
	float m_Zoom;
	float m_ZoomSmoothingTarget;

	bool m_AutoSpecCameraZooming;
	bool m_AutoSpecCamera;
	float m_UserZoomTarget;
	float m_SpecZoomTarget;

	vec2 m_DyncamTargetCameraOffset;
	vec2 m_aDyncamCurrentCameraOffset[NUM_DUMMIES];

	CCamera();
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnRender() override;

	// DDRace

	virtual void OnConsoleInit() override;
	virtual void OnReset() override;

	void SetView(ivec2 Pos, bool Relative = false);
	void GotoSwitch(int Number, int Offset = -1);
	void GotoTele(int Number, int Offset = -1);

	void SetZoom(float Target, int Smoothness, bool IsUser);
	bool ZoomAllowed() const;

	int Deadzone() const;
	int FollowFactor() const;
	int CamType() const { return m_CamType; }

	void UpdateCamera();
	void ResetAutoSpecCamera();
	bool SpectatingPlayer() const { return m_IsSpectatingPlayer; }
	bool CanUseAutoSpecCamera() const;

private:
	static void ConZoomPlus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoomMinus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoom(IConsole::IResult *pResult, void *pUserData);
	static void ConSetView(IConsole::IResult *pResult, void *pUserData);
	static void ConSetViewRelative(IConsole::IResult *pResult, void *pUserData);
	static void ConGotoSwitch(IConsole::IResult *pResult, void *pUserData);
	static void ConGotoTele(IConsole::IResult *pResult, void *pUserData);

	bool m_ForceFreeview;
	vec2 m_ForceFreeviewPos;
	int m_GotoSwitchOffset;
	int m_GotoTeleOffset;
	ivec2 m_GotoSwitchLastPos;
	ivec2 m_GotoTeleLastPos;
	int m_GotoTeleLastNumber = -1;
};

#endif
