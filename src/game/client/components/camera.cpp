/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/tl/string.h>

#include <engine/shared/config.h>

#include <base/math.h>
#include <game/client/component.h>
#include <game/client/gameclient.h>
#include <game/collision.h>

#include "camera.h"
#include "controls.h"

#include <engine/serverbrowser.h>

const float ZoomStep = 0.866025f;

CCamera::CCamera()
{
	m_CamType = CAMTYPE_UNDEFINED;
	m_ZoomSet = false;
	m_Zoom = 1.0f;
	m_Zooming = false;
}

float CCamera::ZoomProgress(float CurrentTime) const
{
	return (CurrentTime - m_ZoomSmoothingStart) / (m_ZoomSmoothingEnd - m_ZoomSmoothingStart);
}

void CCamera::ScaleZoom(float Factor)
{
	float CurrentTarget = m_Zooming ? m_ZoomSmoothingTarget : m_Zoom;
	ChangeZoom(CurrentTarget * Factor);
}

float CCamera::MaxZoomLevel()
{
	return (Graphics()->IsTileBufferingEnabled() ? 60 : 30);
}

float CCamera::MinZoomLevel()
{
	return 0.01f;
}

void CCamera::ChangeZoom(float Target)
{
	if(Target > MaxZoomLevel() || Target < MinZoomLevel())
	{
		return;
	}

	float Now = Client()->LocalTime();
	float Current = m_Zoom;
	float Derivative = 0.0f;
	if(m_Zooming)
	{
		float Progress = ZoomProgress(Now);
		Current = m_ZoomSmoothing.Evaluate(Progress);
		Derivative = m_ZoomSmoothing.Derivative(Progress);
	}

	m_ZoomSmoothingTarget = Target;
	m_ZoomSmoothing = CCubicBezier::With(Current, Derivative, 0, m_ZoomSmoothingTarget);
	m_ZoomSmoothingStart = Now;
	m_ZoomSmoothingEnd = Now + (float)g_Config.m_ClSmoothZoomTime / 1000;

	m_Zooming = true;
}

void CCamera::OnRender()
{
	if(m_Zooming)
	{
		float Time = Client()->LocalTime();
		if(Time >= m_ZoomSmoothingEnd)
		{
			m_Zoom = m_ZoomSmoothingTarget;
			m_Zooming = false;
		}
		else
		{
			m_Zoom = m_ZoomSmoothing.Evaluate(ZoomProgress(Time));
		}
		m_Zoom = clamp(m_Zoom, MinZoomLevel(), MaxZoomLevel());
	}

	if(!(m_pClient->m_Snap.m_SpecInfo.m_Active || GameClient()->m_GameInfo.m_AllowZoom || Client()->State() == IClient::STATE_DEMOPLAYBACK))
	{
		m_ZoomSet = false;
		m_Zoom = 1.0f;
		m_Zooming = false;
	}
	else if(!m_ZoomSet && g_Config.m_ClDefaultZoom != 10)
	{
		m_ZoomSet = true;
		OnReset();
	}

	// update camera center
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		if(m_CamType != CAMTYPE_SPEC)
		{
			m_LastPos[g_Config.m_ClDummy] = m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy];
			m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy] = m_PrevCenter;
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_SPEC;
		}
		m_Center = m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy];
	}
	else
	{
		if(m_CamType != CAMTYPE_PLAYER)
		{
			if((m_LastPos[g_Config.m_ClDummy].x < g_Config.m_ClMouseMinDistance) || (m_LastPos[g_Config.m_ClDummy].x < g_Config.m_ClDyncamMinDistance))
				m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy].x = m_LastPos[g_Config.m_ClDummy].x + g_Config.m_ClMouseMinDistance + g_Config.m_ClDyncamMinDistance;
			else
				m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy] = m_LastPos[g_Config.m_ClDummy];
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_PLAYER;
		}

		vec2 CameraOffset(0, 0);

		float l = length(m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy]);
		if(l > 0.0001f) // make sure that this isn't 0
		{
			float DeadZone = g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
			float FollowFactor = (g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor) / 100.0f;
			float OffsetAmount = maximum(l - DeadZone, 0.0f) * FollowFactor;

			CameraOffset = normalize(m_pClient->m_pControls->m_MousePos[g_Config.m_ClDummy]) * OffsetAmount;
		}

		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
			m_Center = m_pClient->m_Snap.m_SpecInfo.m_Position + CameraOffset;
		else
			m_Center = m_pClient->m_LocalCharacterPos + CameraOffset;
	}

	m_PrevCenter = m_Center;
}

void CCamera::OnConsoleInit()
{
	Console()->Register("zoom+", "", CFGFLAG_CLIENT, ConZoomPlus, this, "Zoom increase");
	Console()->Register("zoom-", "", CFGFLAG_CLIENT, ConZoomMinus, this, "Zoom decrease");
	Console()->Register("zoom", "", CFGFLAG_CLIENT, ConZoomReset, this, "Zoom reset");
}

void CCamera::OnReset()
{
	m_Zoom = pow(ZoomStep, g_Config.m_ClDefaultZoom - 10);
	m_Zooming = false;
}

void CCamera::ConZoomPlus(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active || pSelf->GameClient()->m_GameInfo.m_AllowZoom || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		pSelf->ScaleZoom(ZoomStep);
	}
}
void CCamera::ConZoomMinus(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active || pSelf->GameClient()->m_GameInfo.m_AllowZoom || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		pSelf->ScaleZoom(1 / ZoomStep);
	}
}
void CCamera::ConZoomReset(IConsole::IResult *pResult, void *pUserData)
{
	((CCamera *)pUserData)->ChangeZoom(pow(ZoomStep, g_Config.m_ClDefaultZoom - 10));
}
