/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/shared/snapshot.h>
#include <game/generated/protocol.h>
#include <base/math.h>
#include "extrainfo.h"

bool UseExtraInfo(const CNetObj_Projectile *pProj)
{
  bool ExtraInfoFlag = ((abs(pProj->m_VelY) & (1<<9)) != 0);
  return ExtraInfoFlag;
}

void ExtractInfo(const CNetObj_Projectile *pProj, vec2 *StartPos, vec2 *StartVel, bool IsDDNet)
{
  if(!UseExtraInfo(pProj) || !IsDDNet)
  {
    StartPos->x = pProj->m_X;
    StartPos->y = pProj->m_Y;
    StartVel->x = pProj->m_VelX/100.0f;
    StartVel->y = pProj->m_VelY/100.0f;
  }
  else
  {
    StartPos->x = pProj->m_X/100.0f;
    StartPos->y = pProj->m_Y/100.0f;
    float Angle = pProj->m_VelX/1000000.0f;
    StartVel->x = sin(-Angle);
    StartVel->y = cos(-Angle);
  }
}

void ExtractExtraInfo(const CNetObj_Projectile *pProj, int *Owner, bool *Explosive, int *Bouncing, bool *Freeze)
{
  int Data = pProj->m_VelY;
  if(Owner)
  {
    *Owner = Data & 255;
    if((Data>>8) & 1)
      *Owner = -(*Owner);
  }
  if(Bouncing)
    *Bouncing = (Data>>10) & 3;
  if(Explosive)
    *Explosive = (Data>>12) & 1;
  if(Freeze)
    *Freeze = (Data>>13) & 1;
}

void SnapshotRemoveExtraInfo(unsigned char *pData)
{
  CSnapshot *pSnap = (CSnapshot*) pData;
  for(int Index = 0; Index < pSnap->NumItems(); Index++)
  {
    CSnapshotItem *pItem = pSnap->GetItem(Index);
    if(pItem->Type() == NETOBJTYPE_PROJECTILE)
    {
      CNetObj_Projectile *pProj = (CNetObj_Projectile*) ((void*)pItem->Data());
      if(UseExtraInfo(pProj))
      {
        vec2 Pos;
        vec2 Vel;
        ExtractInfo(pProj, &Pos, &Vel, 1);
        pProj->m_X = Pos.x;
        pProj->m_Y = Pos.y;
        pProj->m_VelX = (int)(Vel.x*100.0f);
        pProj->m_VelY = (int)(Vel.y*100.0f);
      }
    }
  }
}
