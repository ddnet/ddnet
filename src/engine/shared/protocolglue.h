#ifndef ENGINE_SHARED_PROTOCOLGLUE_H
#define ENGINE_SHARED_PROTOCOLGLUE_H

int GameFlags_ClampToSix(int Flags);
int PlayerFlags_SevenToSix(int Flags);
int PlayerFlags_SixToSeven(int Flags);
void PickupType_SevenToSix(int Type7, int &Type6, int &SubType6);
int PickupType_SixToSeven(int Type6, int SubType6);

#endif
