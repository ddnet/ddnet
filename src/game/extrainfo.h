/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EXTRAINFO_H
#define GAME_EXTRAINFO_H

#include <base/vmath.h>

bool UseExtraInfo(const CNetObj_Projectile *pProj);
void ExtractInfo(const CNetObj_Projectile *pProj, vec2 *StartPos, vec2 *StartVel, bool IsDDNet);
void ExtractExtraInfo(const CNetObj_Projectile *pProj, int *Owner, bool *Explosive, int *Bouncing, bool *Freeze);
void SnapshotRemoveExtraInfo(unsigned char *pData);

#endif
