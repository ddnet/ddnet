#include <stdio.h>	// sscanf

#include <engine/console.h>
#include <engine/storage.h>
#include <engine/shared/linereader.h>

#include "auto_map.h"
#include "editor.h"

CAutoMapper::CAutoMapper(CEditor *pEditor)
{
	m_pEditor = pEditor;
	m_FileLoaded = false;
}

void CAutoMapper::Load(const char* pTileName)
{
	char aPath[256];
	str_format(aPath, sizeof(aPath), "editor/%s.rules", pTileName);
	IOHANDLE RulesFile = m_pEditor->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!RulesFile)
		return;

	CLineReader LineReader;
	LineReader.Init(RulesFile);

	CConfiguration *pCurrentConf = 0;
	CIndexRule *pCurrentIndex = 0;

	char aBuf[256];

	// read each line
	while(char *pLine = LineReader.Get())
	{
		// skip blank/empty lines as well as comments
		if(str_length(pLine) > 0 && pLine[0] != '#' && pLine[0] != '\n' && pLine[0] != '\r'
			&& pLine[0] != '\t' && pLine[0] != '\v' && pLine[0] != ' ')
		{
			if(pLine[0]== '[')
			{
				// new configuration, get the name
				pLine++;

				CConfiguration NewConf;
				int ID = m_lConfigs.add(NewConf);
				pCurrentConf = &m_lConfigs[ID];

				str_copy(pCurrentConf->m_aName, pLine, str_length(pLine));
			}
			else
			{
				if(!str_comp_num(pLine, "Index", 5))
				{
					// new index
					int ID = 0;
					char aOrientation1[128] = "";
					char aOrientation2[128] = "";
					char aOrientation3[128] = "";

					sscanf(pLine, "Index %d %127s %127s %127s", &ID, aOrientation1, aOrientation2, aOrientation3);

					CIndexRule NewIndexRule;
					NewIndexRule.m_ID = ID;
					NewIndexRule.m_Flag = 0;
					NewIndexRule.m_RandomValue = 0;
					NewIndexRule.m_DefaultRule = true;

					if(str_length(aOrientation1) > 0)
					{
						if(!str_comp(aOrientation1, "XFLIP"))
							NewIndexRule.m_Flag |= TILEFLAG_VFLIP;
						else if(!str_comp(aOrientation1, "YFLIP"))
							NewIndexRule.m_Flag |= TILEFLAG_HFLIP;
						else if(!str_comp(aOrientation1, "ROTATE"))
							NewIndexRule.m_Flag |= TILEFLAG_ROTATE;
					}

					if(str_length(aOrientation2) > 0)
					{
						if(!str_comp(aOrientation2, "XFLIP"))
							NewIndexRule.m_Flag |= TILEFLAG_VFLIP;
						else if(!str_comp(aOrientation2, "YFLIP"))
							NewIndexRule.m_Flag |= TILEFLAG_HFLIP;
						else if(!str_comp(aOrientation2, "ROTATE"))
							NewIndexRule.m_Flag |= TILEFLAG_ROTATE;
					}

					if(str_length(aOrientation3) > 0)
					{
						if(!str_comp(aOrientation3, "XFLIP"))
							NewIndexRule.m_Flag |= TILEFLAG_VFLIP;
						else if(!str_comp(aOrientation3, "YFLIP"))
							NewIndexRule.m_Flag |= TILEFLAG_HFLIP;
						else if(!str_comp(aOrientation3, "ROTATE"))
							NewIndexRule.m_Flag |= TILEFLAG_ROTATE;
					}

					// add the index rule object and make it current
					int ArrayID = pCurrentConf->m_aIndexRules.add(NewIndexRule);
					pCurrentIndex = &pCurrentConf->m_aIndexRules[ArrayID];
				}
				else if(!str_comp_num(pLine, "Pos", 3) && pCurrentIndex)
				{
					int x = 0, y = 0;
					char aValue[128];
					int Value = CPosRule::NORULE;
					array<CIndexInfo> NewIndexList;

					sscanf(pLine, "Pos %d %d %127s", &x, &y, aValue);

					if(!str_comp(aValue, "EMPTY"))
					{
						Value = CPosRule::EMPTY;
					}
					else if(!str_comp(aValue, "FULL"))
					{
						Value = CPosRule::FULL;
					}
					else if(!str_comp_num(aValue, "INDEX", 5) || !str_comp_num(aValue, "NOTINDEX", 8))
					{
						if(!str_comp_num(aValue, "INDEX", 5))
							Value = CPosRule::INDEX;
						else
							Value = CPosRule::NOTINDEX;

						int pWord = 4;
						while(true) {
							int ID = 0;
							char aOrientation1[128] = "";
							char aOrientation2[128] = "";
							char aOrientation3[128] = "";
							char aOrientation4[128] = "";
							sscanf(str_trim_words(pLine, pWord), "%d %127s %127s %127s %127s", &ID, aOrientation1, aOrientation2, aOrientation3, aOrientation4);

							CIndexInfo NewIndexInfo;
							NewIndexInfo.m_ID = ID;
							NewIndexInfo.m_Flag = 0;

							if(!str_comp(aOrientation1, "OR")) {
								NewIndexList.add(NewIndexInfo);
								pWord += 2;
								continue;
							} else if(str_length(aOrientation1) > 0) {
								if(!str_comp(aOrientation1, "XFLIP"))
									NewIndexInfo.m_Flag |= TILEFLAG_VFLIP;
								else if(!str_comp(aOrientation1, "YFLIP"))
									NewIndexInfo.m_Flag |= TILEFLAG_HFLIP;
								else if(!str_comp(aOrientation1, "ROTATE"))
									NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
							} else {
								NewIndexList.add(NewIndexInfo);
								break;
							}

							if(!str_comp(aOrientation2, "OR")) {
								NewIndexList.add(NewIndexInfo);
								pWord += 3;
								continue;
							} else if(str_length(aOrientation2) > 0) {
								if(!str_comp(aOrientation2, "XFLIP"))
									NewIndexInfo.m_Flag |= TILEFLAG_VFLIP;
								else if(!str_comp(aOrientation2, "YFLIP"))
									NewIndexInfo.m_Flag |= TILEFLAG_HFLIP;
								else if(!str_comp(aOrientation2, "ROTATE"))
									NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
							} else {
								NewIndexList.add(NewIndexInfo);
								break;
							}

							if(!str_comp(aOrientation3, "OR")) {
								NewIndexList.add(NewIndexInfo);
								pWord += 4;
								continue;
							} else if(str_length(aOrientation3) > 0) {
								if(!str_comp(aOrientation3, "XFLIP"))
									NewIndexInfo.m_Flag |= TILEFLAG_VFLIP;
								else if(!str_comp(aOrientation3, "YFLIP"))
									NewIndexInfo.m_Flag |= TILEFLAG_HFLIP;
								else if(!str_comp(aOrientation3, "ROTATE"))
									NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
							} else {
								NewIndexList.add(NewIndexInfo);
								break;
							}

							if(!str_comp(aOrientation4, "OR")) {
								NewIndexList.add(NewIndexInfo);
								pWord += 5;
								continue;
							} else {
								NewIndexList.add(NewIndexInfo);
								break;
							}
						}
					}

					if(Value != CPosRule::NORULE) {
						CPosRule NewPosRule = {x, y, Value, NewIndexList};
						pCurrentIndex->m_aRules.add(NewPosRule);
					}
				}
				else if(!str_comp_num(pLine, "Random", 6) && pCurrentIndex)
				{
					sscanf(pLine, "Random %d", &pCurrentIndex->m_RandomValue);
				}
				else if(!str_comp_num(pLine, "NoDefaultRule", 13) && pCurrentIndex)
				{
					pCurrentIndex->m_DefaultRule = false;
				}
			}
		}
	}

	// add default rule for Pos 0 0 if there is none
	for (int h = 0; h < m_lConfigs.size(); ++h)
	{
		for(int i = 0; i < m_lConfigs[h].m_aIndexRules.size(); ++i)
		{
			bool Found = false;
			for(int j = 0; j < m_lConfigs[h].m_aIndexRules[i].m_aRules.size(); ++j)
			{
				CPosRule *pRule = &m_lConfigs[h].m_aIndexRules[i].m_aRules[j];
				if(pRule && pRule->m_X == 0 && pRule->m_Y == 0)
				{
					Found = true;
					break;
				}
			}
			if(!Found && m_lConfigs[h].m_aIndexRules[i].m_DefaultRule)
			{
				array<CIndexInfo> NewIndexList;
				CPosRule NewPosRule = {0, 0, CPosRule::FULL, NewIndexList};
				m_lConfigs[h].m_aIndexRules[i].m_aRules.add(NewPosRule);
			}
		}
	}

	io_close(RulesFile);

	str_format(aBuf, sizeof(aBuf),"loaded %s", aPath);
	m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);

	m_FileLoaded = true;
}

const char* CAutoMapper::GetConfigName(int Index)
{
	if(Index < 0 || Index >= m_lConfigs.size())
		return "";

	return m_lConfigs[Index].m_aName;
}

void CAutoMapper::Proceed(CLayerTiles *pLayer, int ConfigID)
{
	if(!m_FileLoaded || pLayer->m_Readonly || ConfigID < 0 || ConfigID >= m_lConfigs.size())
		return;

	CConfiguration *pConf = &m_lConfigs[ConfigID];

	if(!pConf->m_aIndexRules.size())
		return;

	CLayerTiles newLayer(pLayer->m_Width, pLayer->m_Height);

	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
		{
			CTile *in = &pLayer->m_pTiles[y*pLayer->m_Width+x];
			CTile *out = &newLayer.m_pTiles[y*pLayer->m_Width+x];
			out->m_Index = in->m_Index;
			out->m_Flags = in->m_Flags;
		}

	// auto map !
	int MaxIndex = pLayer->m_Width*pLayer->m_Height;
	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
		{
			CTile *pTile = &(newLayer.m_pTiles[y*pLayer->m_Width+x]);
			m_pEditor->m_Map.m_Modified = true;

			for(int i = 0; i < pConf->m_aIndexRules.size(); ++i)
			{
				bool RespectRules = true;
				for(int j = 0; j < pConf->m_aIndexRules[i].m_aRules.size() && RespectRules; ++j)
				{
					CPosRule *pRule = &pConf->m_aIndexRules[i].m_aRules[j];
					int CheckIndex = (y+pRule->m_Y)*pLayer->m_Width+(x+pRule->m_X);

					if(CheckIndex < 0 || CheckIndex >= MaxIndex)
						RespectRules = false;
					else
					{
						if(pRule->m_Value == CPosRule::EMPTY)
						{
							if(pLayer->m_pTiles[CheckIndex].m_Index > 0)
								RespectRules = false;
						}
						else if(pRule->m_Value == CPosRule::FULL)
						{
							if(pLayer->m_pTiles[CheckIndex].m_Index == 0)
								RespectRules = false;
						}
 						else if(pRule->m_Value == CPosRule::INDEX)
						{
							bool PosRuleTest = false;
							for(int i = 0; i < pRule->m_aIndexList.size(); ++i) {
								if(pLayer->m_pTiles[CheckIndex].m_Index == pRule->m_aIndexList[i].m_ID && (!pRule->m_aIndexList[i].m_Flag || pLayer->m_pTiles[CheckIndex].m_Flags == pRule->m_aIndexList[i].m_Flag))
									PosRuleTest = true;
							}
							if(!PosRuleTest)
								RespectRules = false;
						}
 						else if(pRule->m_Value == CPosRule::NOTINDEX)
						{
							bool PosRuleTest = true;
							for(int i = 0; i < pRule->m_aIndexList.size(); ++i) {
								if(pLayer->m_pTiles[CheckIndex].m_Index == pRule->m_aIndexList[i].m_ID && (!pRule->m_aIndexList[i].m_Flag || pLayer->m_pTiles[CheckIndex].m_Flags == pRule->m_aIndexList[i].m_Flag))
									PosRuleTest = false;
							}
							if(!PosRuleTest)
								RespectRules = false;
						}
					}
				}

				if(RespectRules &&
					(pConf->m_aIndexRules[i].m_RandomValue <= 1 || (int)((float)rand() / ((float)RAND_MAX + 1) * pConf->m_aIndexRules[i].m_RandomValue) == 1))
				{
					pTile->m_Index = pConf->m_aIndexRules[i].m_ID;
					pTile->m_Flags = pConf->m_aIndexRules[i].m_Flag;
				}
			}
		}

	for(int y = 0; y < pLayer->m_Height; y++)
		for(int x = 0; x < pLayer->m_Width; x++)
		{
			CTile *in = &newLayer.m_pTiles[y*pLayer->m_Width+x];
			CTile *out = &pLayer->m_pTiles[y*pLayer->m_Width+x];
			out->m_Index = in->m_Index;
			out->m_Flags = in->m_Flags;
		}
}
