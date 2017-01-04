/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_ANIMATION_H
#define GAME_ANIMATION_H

#include <base/vmath.h>

void GetAnimationTransform(float GlobalTime, int Env, class CLayers* pLayers, vec2& Position, float& Angle);

#endif
