#include <inttypes.h>
#include <stdio.h> // sscanf

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include "auto_map.h"
#include "editor.h"

// Based on triple32inc from https://github.com/skeeto/hash-prospector/tree/79a6074062a84907df6e45b756134b74e2956760
static uint32_t HashUInt32(uint32_t Num)
{
	Num++;
	Num ^= Num >> 17;
	Num *= 0xed5ad4bbu;
	Num ^= Num >> 11;
	Num *= 0xac4c1b51u;
	Num ^= Num >> 15;
	Num *= 0x31848babu;
	Num ^= Num >> 14;
	return Num;
}

#define HASH_MAX 65536

static int HashLocation(uint32_t Seed, uint32_t Run, uint32_t Rule, uint32_t X, uint32_t Y)
{
	const uint32_t Prime = 31;
	uint32_t Hash = 1;
	Hash = Hash * Prime + HashUInt32(Seed);
	Hash = Hash * Prime + HashUInt32(Run);
	Hash = Hash * Prime + HashUInt32(Rule);
	Hash = Hash * Prime + HashUInt32(X);
	Hash = Hash * Prime + HashUInt32(Y);
	Hash = HashUInt32(Hash * Prime); // Just to double-check that values are well-distributed
	return Hash % HASH_MAX;
}

CAutoMapper::CAutoMapper(CEditor *pEditor)
{
	m_pEditor = pEditor;
	m_FileLoaded = false;
}

void CAutoMapper::Load(const char *pTileName)
{
	char aPath[256];
	str_format(aPath, sizeof(aPath), "editor/%s.rules", pTileName);
	IOHANDLE RulesFile = m_pEditor->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!RulesFile)
		return;

	CLineReader LineReader;
	LineReader.Init(RulesFile);

	CConfiguration *pCurrentConf = 0;
	CRun *pCurrentRun = 0;
	CIndexRule *pCurrentIndex = 0;

	char aBuf[256];

	// read each line
	while(char *pLine = LineReader.Get())
	{
		// skip blank/empty lines as well as comments
		if(str_length(pLine) > 0 && pLine[0] != '#' && pLine[0] != '\n' && pLine[0] != '\r' && pLine[0] != '\t' && pLine[0] != '\v' && pLine[0] != ' ')
		{
			if(pLine[0] == '[')
			{
				// new configuration, get the name
				pLine++;
				CConfiguration NewConf;
				NewConf.m_aName[0] = '\0';
				NewConf.m_StartX = 0;
				NewConf.m_StartY = 0;
				NewConf.m_EndX = 0;
				NewConf.m_EndY = 0;
				int ConfigurationID = m_lConfigs.add(NewConf);
				pCurrentConf = &m_lConfigs[ConfigurationID];
				str_copy(pCurrentConf->m_aName, pLine, str_length(pLine));

				// add start run
				CRun NewRun;
				NewRun.m_AutomapCopy = true;
				int RunID = pCurrentConf->m_aRuns.add(NewRun);
				pCurrentRun = &pCurrentConf->m_aRuns[RunID];
			}
			else if(str_startswith(pLine, "NewRun"))
			{
				// add new run
				CRun NewRun;
				NewRun.m_AutomapCopy = true;
				int RunID = pCurrentConf->m_aRuns.add(NewRun);
				pCurrentRun = &pCurrentConf->m_aRuns[RunID];
			}
			else if(str_startswith(pLine, "Index") && pCurrentRun)
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
				NewIndexRule.m_RandomProbability = 1.0f;
				NewIndexRule.m_DefaultRule = true;
				NewIndexRule.m_SkipEmpty = false;
				NewIndexRule.m_SkipFull = false;

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
				int IndexRuleID = pCurrentRun->m_aIndexRules.add(NewIndexRule);
				pCurrentIndex = &pCurrentRun->m_aIndexRules[IndexRuleID];
			}
			else if(str_startswith(pLine, "Pos") && pCurrentIndex)
			{
				int x = 0, y = 0;
				char aValue[128];
				int Value = CPosRule::NORULE;
				array<CIndexInfo> NewIndexList;

				sscanf(pLine, "Pos %d %d %127s", &x, &y, aValue);

				if(!str_comp(aValue, "EMPTY"))
				{
					Value = CPosRule::INDEX;
					CIndexInfo NewIndexInfo = {0, 0, false};
					NewIndexList.add(NewIndexInfo);
				}
				else if(!str_comp(aValue, "FULL"))
				{
					Value = CPosRule::NOTINDEX;
					CIndexInfo NewIndexInfo1 = {0, 0, false};
					//CIndexInfo NewIndexInfo2 = {-1, 0};
					NewIndexList.add(NewIndexInfo1);
					//NewIndexList.add(NewIndexInfo2);
				}
				else if(!str_comp(aValue, "INDEX") || !str_comp(aValue, "NOTINDEX"))
				{
					if(!str_comp(aValue, "INDEX"))
						Value = CPosRule::INDEX;
					else
						Value = CPosRule::NOTINDEX;

					int pWord = 4;
					while(true)
					{
						int ID = 0;
						char aOrientation1[128] = "";
						char aOrientation2[128] = "";
						char aOrientation3[128] = "";
						char aOrientation4[128] = "";
						sscanf(str_trim_words(pLine, pWord), "%d %127s %127s %127s %127s", &ID, aOrientation1, aOrientation2, aOrientation3, aOrientation4);

						CIndexInfo NewIndexInfo;
						NewIndexInfo.m_ID = ID;
						NewIndexInfo.m_Flag = 0;
						NewIndexInfo.m_TestFlag = false;

						if(!str_comp(aOrientation1, "OR"))
						{
							NewIndexList.add(NewIndexInfo);
							pWord += 2;
							continue;
						}
						else if(str_length(aOrientation1) > 0)
						{
							NewIndexInfo.m_TestFlag = true;
							if(!str_comp(aOrientation1, "XFLIP"))
								NewIndexInfo.m_Flag = TILEFLAG_VFLIP;
							else if(!str_comp(aOrientation1, "YFLIP"))
								NewIndexInfo.m_Flag = TILEFLAG_HFLIP;
							else if(!str_comp(aOrientation1, "ROTATE"))
								NewIndexInfo.m_Flag = TILEFLAG_ROTATE;
							else if(!str_comp(aOrientation1, "NONE"))
								NewIndexInfo.m_Flag = 0;
							else
								NewIndexInfo.m_TestFlag = false;
						}
						else
						{
							NewIndexList.add(NewIndexInfo);
							break;
						}

						if(!str_comp(aOrientation2, "OR"))
						{
							NewIndexList.add(NewIndexInfo);
							pWord += 3;
							continue;
						}
						else if(str_length(aOrientation2) > 0 && NewIndexInfo.m_Flag != 0)
						{
							if(!str_comp(aOrientation2, "XFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_VFLIP;
							else if(!str_comp(aOrientation2, "YFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_HFLIP;
							else if(!str_comp(aOrientation2, "ROTATE"))
								NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
						}
						else
						{
							NewIndexList.add(NewIndexInfo);
							break;
						}

						if(!str_comp(aOrientation3, "OR"))
						{
							NewIndexList.add(NewIndexInfo);
							pWord += 4;
							continue;
						}
						else if(str_length(aOrientation3) > 0 && NewIndexInfo.m_Flag != 0)
						{
							if(!str_comp(aOrientation3, "XFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_VFLIP;
							else if(!str_comp(aOrientation3, "YFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_HFLIP;
							else if(!str_comp(aOrientation3, "ROTATE"))
								NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
						}
						else
						{
							NewIndexList.add(NewIndexInfo);
							break;
						}

						if(!str_comp(aOrientation4, "OR"))
						{
							NewIndexList.add(NewIndexInfo);
							pWord += 5;
							continue;
						}
						else
						{
							NewIndexList.add(NewIndexInfo);
							break;
						}
					}
				}

				if(Value != CPosRule::NORULE)
				{
					CPosRule NewPosRule = {x, y, Value, NewIndexList};
					pCurrentIndex->m_aRules.add(NewPosRule);

					pCurrentConf->m_StartX = minimum(pCurrentConf->m_StartX, NewPosRule.m_X);
					pCurrentConf->m_StartY = minimum(pCurrentConf->m_StartY, NewPosRule.m_Y);
					pCurrentConf->m_EndX = maximum(pCurrentConf->m_EndX, NewPosRule.m_X);
					pCurrentConf->m_EndY = maximum(pCurrentConf->m_EndY, NewPosRule.m_Y);

					if(x == 0 && y == 0)
					{
						for(int i = 0; i < NewIndexList.size(); ++i)
						{
							if(Value == CPosRule::INDEX && NewIndexList[i].m_ID == 0)
								pCurrentIndex->m_SkipFull = true;
							else
								pCurrentIndex->m_SkipEmpty = true;
						}
					}
				}
			}
			else if(str_startswith(pLine, "Random") && pCurrentIndex)
			{
				float Value;
				char Specifier = ' ';
				sscanf(pLine, "Random %f%c", &Value, &Specifier);
				if(Specifier == '%')
				{
					pCurrentIndex->m_RandomProbability = Value / 100.0f;
				}
				else
				{
					pCurrentIndex->m_RandomProbability = 1.0f / Value;
				}
			}
			else if(str_startswith(pLine, "NoDefaultRule") && pCurrentIndex)
			{
				pCurrentIndex->m_DefaultRule = false;
			}
			else if(str_startswith(pLine, "NoLayerCopy") && pCurrentRun)
			{
				pCurrentRun->m_AutomapCopy = false;
			}
		}
	}

	// add default rule for Pos 0 0 if there is none
	for(int g = 0; g < m_lConfigs.size(); ++g)
	{
		for(int h = 0; h < m_lConfigs[g].m_aRuns.size(); ++h)
		{
			for(int i = 0; i < m_lConfigs[g].m_aRuns[h].m_aIndexRules.size(); ++i)
			{
				CIndexRule *pIndexRule = &m_lConfigs[g].m_aRuns[h].m_aIndexRules[i];
				bool Found = false;
				for(int j = 0; j < pIndexRule->m_aRules.size(); ++j)
				{
					CPosRule *pRule = &pIndexRule->m_aRules[j];
					if(pRule && pRule->m_X == 0 && pRule->m_Y == 0)
					{
						Found = true;
						break;
					}
				}
				if(!Found && pIndexRule->m_DefaultRule)
				{
					array<CIndexInfo> NewIndexList;
					CIndexInfo NewIndexInfo = {0, 0, false};
					NewIndexList.add(NewIndexInfo);
					CPosRule NewPosRule = {0, 0, CPosRule::NOTINDEX, NewIndexList};
					pIndexRule->m_aRules.add(NewPosRule);

					pIndexRule->m_SkipEmpty = true;
					pIndexRule->m_SkipFull = false;
				}
				if(pIndexRule->m_SkipEmpty && pIndexRule->m_SkipFull)
				{
					pIndexRule->m_SkipEmpty = false;
					pIndexRule->m_SkipFull = false;
				}
			}
		}
	}

	io_close(RulesFile);

	str_format(aBuf, sizeof(aBuf), "loaded %s", aPath);
	m_pEditor->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor", aBuf);

	m_FileLoaded = true;
}

const char *CAutoMapper::GetConfigName(int Index)
{
	if(Index < 0 || Index >= m_lConfigs.size())
		return "";

	return m_lConfigs[Index].m_aName;
}

void CAutoMapper::ProceedLocalized(CLayerTiles *pLayer, int ConfigID, int Seed, int X, int Y, int Width, int Height)
{
	if(!m_FileLoaded || pLayer->m_Readonly || ConfigID < 0 || ConfigID >= m_lConfigs.size())
		return;

	if(Width < 0)
		Width = pLayer->m_Width;

	if(Height < 0)
		Height = pLayer->m_Height;

	CConfiguration *pConf = &m_lConfigs[ConfigID];

	int CommitFromX = clamp(X + pConf->m_StartX, 0, pLayer->m_Width);
	int CommitFromY = clamp(Y + pConf->m_StartY, 0, pLayer->m_Height);
	int CommitToX = clamp(X + Width + pConf->m_EndX, 0, pLayer->m_Width);
	int CommitToY = clamp(Y + Height + pConf->m_EndY, 0, pLayer->m_Height);

	int UpdateFromX = clamp(X + 3 * pConf->m_StartX, 0, pLayer->m_Width);
	int UpdateFromY = clamp(Y + 3 * pConf->m_StartY, 0, pLayer->m_Height);
	int UpdateToX = clamp(X + Width + 3 * pConf->m_EndX, 0, pLayer->m_Width);
	int UpdateToY = clamp(Y + Height + 3 * pConf->m_EndY, 0, pLayer->m_Height);

	CLayerTiles *pUpdateLayer;
	if(UpdateFromX != 0 || UpdateFromY != 0 || UpdateToX != pLayer->m_Width || UpdateToY != pLayer->m_Width)
	{ // Needs a layer to work on
		pUpdateLayer = new CLayerTiles(UpdateToX - UpdateFromX, UpdateToY - UpdateFromY);

		for(int y = UpdateFromY; y < UpdateToY; y++)
		{
			for(int x = UpdateFromX; x < UpdateToX; x++)
			{
				CTile *in = &pLayer->m_pTiles[y * pLayer->m_Width + x];
				CTile *out = &pUpdateLayer->m_pTiles[(y - UpdateFromY) * pUpdateLayer->m_Width + x - UpdateFromX];
				out->m_Index = in->m_Index;
				out->m_Flags = in->m_Flags;
			}
		}
	}
	else
	{
		pUpdateLayer = pLayer;
	}

	Proceed(pUpdateLayer, ConfigID, Seed, UpdateFromX, UpdateFromY);

	for(int y = CommitFromY; y < CommitToY; y++)
	{
		for(int x = CommitFromX; x < CommitToX; x++)
		{
			CTile *in = &pUpdateLayer->m_pTiles[(y - UpdateFromY) * pUpdateLayer->m_Width + x - UpdateFromX];
			CTile *out = &pLayer->m_pTiles[y * pLayer->m_Width + x];
			out->m_Index = in->m_Index;
			out->m_Flags = in->m_Flags;
		}
	}
}

void CAutoMapper::Proceed(CLayerTiles *pLayer, int ConfigID, int Seed, int SeedOffsetX, int SeedOffsetY)
{
	if(!m_FileLoaded || pLayer->m_Readonly || ConfigID < 0 || ConfigID >= m_lConfigs.size())
		return;

	if(Seed == 0)
		Seed = rand();

	CConfiguration *pConf = &m_lConfigs[ConfigID];

	// for every run: copy tiles, automap, overwrite tiles
	for(int h = 0; h < pConf->m_aRuns.size(); ++h)
	{
		CRun *pRun = &pConf->m_aRuns[h];

		// don't make copy if it's requested
		CLayerTiles *pReadLayer;
		if(pRun->m_AutomapCopy)
		{
			pReadLayer = new CLayerTiles(pLayer->m_Width, pLayer->m_Height);

			for(int y = 0; y < pLayer->m_Height; y++)
			{
				for(int x = 0; x < pLayer->m_Width; x++)
				{
					CTile *in = &pLayer->m_pTiles[y * pLayer->m_Width + x];
					CTile *out = &pReadLayer->m_pTiles[y * pLayer->m_Width + x];
					out->m_Index = in->m_Index;
					out->m_Flags = in->m_Flags;
				}
			}
		}
		else
		{
			pReadLayer = pLayer;
		}

		// auto map
		for(int y = 0; y < pLayer->m_Height; y++)
		{
			for(int x = 0; x < pLayer->m_Width; x++)
			{
				CTile *pTile = &(pLayer->m_pTiles[y * pLayer->m_Width + x]);
				m_pEditor->m_Map.m_Modified = true;

				for(int i = 0; i < pRun->m_aIndexRules.size(); ++i)
				{
					CIndexRule *pIndexRule = &pRun->m_aIndexRules[i];
					if(pIndexRule->m_SkipEmpty && pTile->m_Index == 0) // skip empty tiles
						continue;
					if(pIndexRule->m_SkipFull && pTile->m_Index != 0) // skip full tiles
						continue;

					bool RespectRules = true;
					for(int j = 0; j < pIndexRule->m_aRules.size() && RespectRules; ++j)
					{
						CPosRule *pRule = &pIndexRule->m_aRules[j];

						int CheckIndex, CheckFlags;
						int CheckX = x + pRule->m_X;
						int CheckY = y + pRule->m_Y;
						if(CheckX >= 0 && CheckX < pLayer->m_Width && CheckY >= 0 && CheckY < pLayer->m_Height)
						{
							int CheckTile = CheckY * pLayer->m_Width + CheckX;
							CheckIndex = pReadLayer->m_pTiles[CheckTile].m_Index;
							CheckFlags = pReadLayer->m_pTiles[CheckTile].m_Flags & (TILEFLAG_ROTATE | TILEFLAG_VFLIP | TILEFLAG_HFLIP);
						}
						else
						{
							CheckIndex = -1;
							CheckFlags = 0;
						}

						if(pRule->m_Value == CPosRule::INDEX)
						{
							RespectRules = false;
							for(int i = 0; i < pRule->m_aIndexList.size(); ++i)
							{
								if(CheckIndex == pRule->m_aIndexList[i].m_ID && (!pRule->m_aIndexList[i].m_TestFlag || CheckFlags == pRule->m_aIndexList[i].m_Flag))
								{
									RespectRules = true;
									break;
								}
							}
						}
						else if(pRule->m_Value == CPosRule::NOTINDEX)
						{
							for(int i = 0; i < pRule->m_aIndexList.size(); ++i)
							{
								if(CheckIndex == pRule->m_aIndexList[i].m_ID && (!pRule->m_aIndexList[i].m_TestFlag || CheckFlags == pRule->m_aIndexList[i].m_Flag))
								{
									RespectRules = false;
									break;
								}
							}
						}
					}

					if(RespectRules &&
						(pIndexRule->m_RandomProbability >= 1.0f || HashLocation(Seed, h, i, x + SeedOffsetX, y + SeedOffsetY) < HASH_MAX * pIndexRule->m_RandomProbability))
					{
						pTile->m_Index = pIndexRule->m_ID;
						pTile->m_Flags = pIndexRule->m_Flag;
					}
				}
			}
		}

		// clean-up
		if(pRun->m_AutomapCopy)
			delete pReadLayer;
	}
}
