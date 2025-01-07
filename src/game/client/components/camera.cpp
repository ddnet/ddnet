/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/shared/config.h>

#include <base/log.h>
#include <base/math.h>
#include <base/vmath.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include "camera.h"
#include "controls.h"

#include <limits>

CCamera::CCamera()
{
	m_CamType = CAMTYPE_UNDEFINED;
	m_ZoomSet = false;
	m_Zoom = 1.0f;
	m_Zooming = false;
	m_ForceFreeview = false;
	m_GotoSwitchOffset = 0;
	m_GotoTeleOffset = 0;
	m_GotoSwitchLastPos = ivec2(-1, -1);
	m_GotoTeleLastPos = ivec2(-1, -1);

	mem_zero(m_aLastPos, sizeof(m_aLastPos));
	m_PrevCenter = vec2(0, 0);
	m_Center = vec2(0, 0);

	m_PrevSpecId = -1;
	m_WasSpectating = false;

	m_CameraSmoothing = false;

	m_LastMousePos = vec2(0, 0);
	m_DyncamTargetCameraOffset = vec2(0, 0);
	mem_zero(m_aDyncamCurrentCameraOffset, sizeof(m_aDyncamCurrentCameraOffset));
	m_DyncamSmoothingSpeedBias = 0.5f;
}

float CCamera::CameraSmoothingProgress(float CurrentTime) const
{
	float Progress = (CurrentTime - m_CameraSmoothingStart) / (m_CameraSmoothingEnd - m_CameraSmoothingStart);
	return 1.0 - std::pow(2.0, -10.0 * Progress);
}

float CCamera::ZoomProgress(float CurrentTime) const
{
	return (CurrentTime - m_ZoomSmoothingStart) / (m_ZoomSmoothingEnd - m_ZoomSmoothingStart);
}

void CCamera::ScaleZoom(float Factor)
{
	float CurrentTarget = m_Zooming ? m_ZoomSmoothingTarget : m_Zoom;
	ChangeZoom(CurrentTarget * Factor, m_pClient->m_Snap.m_SpecInfo.m_Active && GameClient()->m_MultiViewActivated ? g_Config.m_ClMultiViewZoomSmoothness : g_Config.m_ClSmoothZoomTime);
}

float CCamera::MaxZoomLevel()
{
	return (g_Config.m_ClLimitMaxZoomLevel) ? ((Graphics()->IsTileBufferingEnabled() ? 240 : 30)) : std::numeric_limits<float>::max();
}

float CCamera::MinZoomLevel()
{
	return 0.01f;
}

void CCamera::ChangeZoom(float Target, int Smoothness)
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
	m_ZoomSmoothingEnd = Now + (float)Smoothness / 1000;

	m_Zooming = true;
}

void CCamera::UpdateCamera()
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
			const float OldLevel = m_Zoom;
			m_Zoom = m_ZoomSmoothing.Evaluate(ZoomProgress(Time));
			if((OldLevel < m_ZoomSmoothingTarget && m_Zoom > m_ZoomSmoothingTarget) || (OldLevel > m_ZoomSmoothingTarget && m_Zoom < m_ZoomSmoothingTarget))
			{
				m_Zoom = m_ZoomSmoothingTarget;
				m_Zooming = false;
			}
		}
		m_Zoom = clamp(m_Zoom, MinZoomLevel(), MaxZoomLevel());
	}

	if(!ZoomAllowed())
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

	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
		return;

	float DeltaTime = Client()->RenderFrameTime();

	if(g_Config.m_ClDyncamSmoothness > 0)
	{
		float CameraSpeed = (1.0f - (g_Config.m_ClDyncamSmoothness / 100.0f)) * 9.5f + 0.5f;
		float CameraStabilizingFactor = 1 + g_Config.m_ClDyncamStabilizing / 100.0f;

		m_DyncamSmoothingSpeedBias += CameraSpeed * DeltaTime;
		if(g_Config.m_ClDyncam)
		{
			m_DyncamSmoothingSpeedBias -= length(m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] - m_LastMousePos) * std::log10(CameraStabilizingFactor) * 0.02f;
			m_DyncamSmoothingSpeedBias = clamp(m_DyncamSmoothingSpeedBias, 0.5f, CameraSpeed);
		}
		else
		{
			m_DyncamSmoothingSpeedBias = maximum(5.0f, CameraSpeed); // make sure toggle back is fast
		}
	}

	m_DyncamTargetCameraOffset = vec2(0, 0);
	vec2 MousePos = m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy];
	float l = length(MousePos);
	if(l > 0.0001f) // make sure that this isn't 0
	{
		float OffsetAmount = maximum(l - Deadzone(), 0.0f) * (FollowFactor() / 100.0f);
		m_DyncamTargetCameraOffset = normalize_pre_length(MousePos, l) * OffsetAmount;
	}

	m_LastMousePos = MousePos;
	if(g_Config.m_ClDyncamSmoothness > 0)
		m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy] += (m_DyncamTargetCameraOffset - m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy]) * minimum(DeltaTime * m_DyncamSmoothingSpeedBias, 1.0f);
	else
		m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy] = m_DyncamTargetCameraOffset;
}

void CCamera::OnRender()
{
	if(m_CameraSmoothing)
	{
		if(!m_pClient->m_Snap.m_SpecInfo.m_Active)
		{
			m_Center = m_CameraSmoothingTarget;
			m_CameraSmoothing = false;
		}
		else
		{
			float Time = Client()->LocalTime();
			if(Time >= m_CameraSmoothingEnd)
			{
				m_Center = m_CameraSmoothingTarget;
				m_CameraSmoothing = false;
			}
			else
			{
				m_CameraSmoothingCenter = vec2(m_CameraSmoothingBezierX.Evaluate(CameraSmoothingProgress(Time)), m_CameraSmoothingBezierY.Evaluate(CameraSmoothingProgress(Time)));
				if(distance(m_CameraSmoothingCenter, m_CameraSmoothingTarget) <= 0.1f)
				{
					m_Center = m_CameraSmoothingTarget;
					m_CameraSmoothing = false;
				}
			}
		}
	}

	// update camera center
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		if(m_CamType != CAMTYPE_SPEC)
		{
			m_aLastPos[g_Config.m_ClDummy] = m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy];
			m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] = m_PrevCenter;
			m_pClient->m_Controls.ClampMousePos();
			m_CamType = CAMTYPE_SPEC;
		}
		m_Center = m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy];
	}
	else
	{
		if(m_CamType != CAMTYPE_PLAYER)
		{
			m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] = m_aLastPos[g_Config.m_ClDummy];
			m_pClient->m_Controls.ClampMousePos();
			m_CamType = CAMTYPE_PLAYER;
		}

		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
			m_Center = m_pClient->m_Snap.m_SpecInfo.m_Position + m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy];
		else
			m_Center = m_pClient->m_LocalCharacterPos + m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy];
	}

	if(m_ForceFreeview && m_CamType == CAMTYPE_SPEC)
	{
		m_Center = m_pClient->m_Controls.m_aMousePos[g_Config.m_ClDummy] = m_ForceFreeviewPos;
		m_ForceFreeview = false;
	}
	else
		m_ForceFreeviewPos = m_Center;

	const int SpecId = m_pClient->m_Snap.m_SpecInfo.m_SpectatorId;

	// start smoothing from the current position when the target changes
	if(m_CameraSmoothing && SpecId != m_PrevSpecId)
		m_CameraSmoothing = false;

	if(m_pClient->m_Snap.m_SpecInfo.m_Active &&
		(SpecId != m_PrevSpecId ||
			(m_CameraSmoothing && m_CameraSmoothingTarget != m_Center)) && // the target is moving during camera smoothing
		!(!m_WasSpectating && m_Center != m_PrevCenter) && // dont smooth when starting to spectate
		m_CamType != CAMTYPE_SPEC &&
		!GameClient()->m_MultiViewActivated)
	{
		float Now = Client()->LocalTime();
		if(!m_CameraSmoothing)
			m_CenterBeforeSmoothing = m_PrevCenter;

		vec2 Derivative = {0.f, 0.f};
		if(m_CameraSmoothing)
		{
			float Progress = CameraSmoothingProgress(Now);
			Derivative.x = m_CameraSmoothingBezierX.Derivative(Progress);
			Derivative.y = m_CameraSmoothingBezierY.Derivative(Progress);
		}

		m_CameraSmoothingTarget = m_Center;
		m_CameraSmoothingBezierX = CCubicBezier::With(m_CenterBeforeSmoothing.x, Derivative.x, 0, m_CameraSmoothingTarget.x);
		m_CameraSmoothingBezierY = CCubicBezier::With(m_CenterBeforeSmoothing.y, Derivative.y, 0, m_CameraSmoothingTarget.y);

		if(!m_CameraSmoothing)
		{
			m_CameraSmoothingStart = Now;
			m_CameraSmoothingEnd = Now + (float)g_Config.m_ClSmoothSpectatingTime / 1000.0f;
		}

		if(!m_CameraSmoothing)
			m_CameraSmoothingCenter = m_PrevCenter;

		m_CameraSmoothing = true;
	}

	if(m_CameraSmoothing)
		m_Center = m_CameraSmoothingCenter;

	m_PrevCenter = m_Center;
	m_PrevSpecId = SpecId;
	m_WasSpectating = m_pClient->m_Snap.m_SpecInfo.m_Active;
}

void CCamera::OnConsoleInit()
{
	Console()->Register("zoom+", "?f[amount]", CFGFLAG_CLIENT, ConZoomPlus, this, "Zoom increase");
	Console()->Register("zoom-", "?f[amount]", CFGFLAG_CLIENT, ConZoomMinus, this, "Zoom decrease");
	Console()->Register("zoom", "?f", CFGFLAG_CLIENT, ConZoom, this, "Change zoom");
	Console()->Register("set_view", "i[x]i[y]", CFGFLAG_CLIENT, ConSetView, this, "Set camera position to x and y in the map");
	Console()->Register("set_view_relative", "i[x]i[y]", CFGFLAG_CLIENT, ConSetViewRelative, this, "Set camera position relative to current view in the map");
	Console()->Register("goto_switch", "i[number]?i[offset]", CFGFLAG_CLIENT, ConGotoSwitch, this, "View switch found (at offset) with given number");
	Console()->Register("goto_tele", "i[number]?i[offset]", CFGFLAG_CLIENT, ConGotoTele, this, "View tele found (at offset) with given number");
}

void CCamera::OnReset()
{
	m_CameraSmoothing = false;

	m_Zoom = std::pow(CCamera::ZOOM_STEP, g_Config.m_ClDefaultZoom - 10);
	m_Zooming = false;
}

void CCamera::ConZoomPlus(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(!pSelf->ZoomAllowed())
		return;

	float ZoomAmount = pResult->NumArguments() ? pResult->GetFloat(0) : 1.0f;

	pSelf->ScaleZoom(std::pow(CCamera::ZOOM_STEP, ZoomAmount));

	if(pSelf->GameClient()->m_MultiViewActivated)
		pSelf->GameClient()->m_MultiViewPersonalZoom += ZoomAmount;
}
void CCamera::ConZoomMinus(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(!pSelf->ZoomAllowed())
		return;

	float ZoomAmount = pResult->NumArguments() ? pResult->GetFloat(0) : 1.0f;
	ZoomAmount *= -1.0f;

	pSelf->ScaleZoom(std::pow(CCamera::ZOOM_STEP, ZoomAmount));

	if(pSelf->GameClient()->m_MultiViewActivated)
		pSelf->GameClient()->m_MultiViewPersonalZoom += ZoomAmount;
}
void CCamera::ConZoom(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	if(!pSelf->ZoomAllowed())
		return;

	float TargetLevel = pResult->NumArguments() ? pResult->GetFloat(0) : g_Config.m_ClDefaultZoom;
	pSelf->ChangeZoom(std::pow(CCamera::ZOOM_STEP, TargetLevel - 10.0f), pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active && pSelf->GameClient()->m_MultiViewActivated ? g_Config.m_ClMultiViewZoomSmoothness : g_Config.m_ClSmoothZoomTime);

	if(pSelf->GameClient()->m_MultiViewActivated && pSelf->m_pClient->m_Snap.m_SpecInfo.m_Active)
		pSelf->GameClient()->m_MultiViewPersonalZoom = TargetLevel - 10.0f;
}
void CCamera::ConSetView(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	// wait until free view camera type to update the position
	pSelf->SetView(ivec2(pResult->GetInteger(0), pResult->GetInteger(1)));
}
void CCamera::ConSetViewRelative(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	// wait until free view camera type to update the position
	pSelf->SetView(ivec2(pResult->GetInteger(0), pResult->GetInteger(1)), true);
}
void CCamera::ConGotoSwitch(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	pSelf->GotoSwitch(pResult->GetInteger(0), pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1);
}
void CCamera::ConGotoTele(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pSelf = (CCamera *)pUserData;
	pSelf->GotoTele(pResult->GetInteger(0), pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1);
}

void CCamera::SetView(ivec2 Pos, bool Relative)
{
	vec2 RealPos = vec2(Pos.x * 32.0, Pos.y * 32.0);
	vec2 UntestedViewPos = Relative ? m_ForceFreeviewPos + RealPos : RealPos;

	m_ForceFreeview = true;

	m_ForceFreeviewPos = vec2(
		clamp(UntestedViewPos.x, 200.0f, Collision()->GetWidth() * 32 - 200.0f),
		clamp(UntestedViewPos.y, 200.0f, Collision()->GetWidth() * 32 - 200.0f));
}

void CCamera::GotoSwitch(int Number, int Offset)
{
	if(Collision()->SwitchLayer() == nullptr)
		return;

	int Match = -1;
	ivec2 MatchPos = ivec2(-1, -1);

	auto FindTile = [this, &Match, &MatchPos, &Number, &Offset]() {
		for(int x = 0; x < Collision()->GetWidth(); x++)
		{
			for(int y = 0; y < Collision()->GetHeight(); y++)
			{
				int i = y * Collision()->GetWidth() + x;
				if(Number == Collision()->GetSwitchNumber(i))
				{
					Match++;
					if(Offset != -1)
					{
						if(Match == Offset)
						{
							MatchPos = ivec2(x, y);
							m_GotoSwitchOffset = Match;
							return;
						}
						continue;
					}
					MatchPos = ivec2(x, y);
					if(Match == m_GotoSwitchOffset)
						return;
				}
			}
		}
	};
	FindTile();

	if(MatchPos == ivec2(-1, -1))
		return;
	if(Match < m_GotoSwitchOffset)
		m_GotoSwitchOffset = -1;
	SetView(MatchPos);
	m_GotoSwitchOffset++;
}

void CCamera::GotoTele(int Number, int Offset)
{
	if(Collision()->TeleLayer() == nullptr)
		return;
	Number--;

	if(m_GotoTeleLastNumber != Number)
		m_GotoTeleLastPos = ivec2(-1, -1);

	ivec2 MatchPos = ivec2(-1, -1);
	const size_t NumTeles = Collision()->TeleAllSize(Number);
	if(!NumTeles)
	{
		log_error("camera", "No teleporter with number %d found.", Number + 1);
		return;
	}

	if(Offset != -1 || m_GotoTeleLastPos == ivec2(-1, -1))
	{
		if((size_t)Offset >= NumTeles || Offset < 0)
			Offset = 0;
		vec2 Tele = Collision()->TeleAllGet(Number, Offset);
		MatchPos = ivec2(Tele.x / 32, Tele.y / 32);
		m_GotoTeleOffset = Offset;
	}
	else
	{
		bool FullRound = false;
		do
		{
			vec2 Tele = Collision()->TeleAllGet(Number, m_GotoTeleOffset);
			MatchPos = ivec2(Tele.x / 32, Tele.y / 32);
			m_GotoTeleOffset++;
			if((size_t)m_GotoTeleOffset >= NumTeles)
			{
				m_GotoTeleOffset = 0;
				if(FullRound)
				{
					MatchPos = m_GotoTeleLastPos;
					break;
				}
				else
				{
					FullRound = true;
				}
			}
		} while(distance(m_GotoTeleLastPos, MatchPos) < 10.0f);
	}

	if(MatchPos == ivec2(-1, -1))
		return;
	m_GotoTeleLastPos = MatchPos;
	m_GotoTeleLastNumber = Number;
	SetView(MatchPos);
}

void CCamera::SetZoom(float Target, int Smoothness)
{
	ChangeZoom(Target, Smoothness);
}

bool CCamera::ZoomAllowed() const
{
	return GameClient()->m_Snap.m_SpecInfo.m_Active ||
	       GameClient()->m_GameInfo.m_AllowZoom ||
	       Client()->State() == IClient::STATE_DEMOPLAYBACK;
}

int CCamera::Deadzone() const
{
	return g_Config.m_ClDyncam ? g_Config.m_ClDyncamDeadzone : g_Config.m_ClMouseDeadzone;
}

int CCamera::FollowFactor() const
{
	return g_Config.m_ClDyncam ? g_Config.m_ClDyncamFollowFactor : g_Config.m_ClMouseFollowfactor;
}
