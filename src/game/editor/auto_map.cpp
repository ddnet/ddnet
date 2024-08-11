#include <cinttypes>
#include <cstdio> // sscanf

#include <engine/console.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <game/editor/mapitems/layer_tiles.h>
#include <game/mapitems.h>

#include "auto_map.h"
#include "editor_actions.h"

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
	OnInit(pEditor);
}

void CAutoMapper::Load(const char *pTileName)
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "editor/automap/%s.rules", pTileName);
	CLineReader LineReader;
	if(!LineReader.OpenFile(Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL)))
	{
		char aBuf[IO_MAX_PATH_LENGTH + 32];
		str_format(aBuf, sizeof(aBuf), "failed to load %s", aPath);
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor/automap", aBuf);
		return;
	}

	CConfiguration *pCurrentConf = nullptr;
	CRun *pCurrentRun = nullptr;
	CIndexRule *pCurrentIndex = nullptr;

	// read each line
	while(const char *pLine = LineReader.Get())
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
				m_vConfigs.push_back(NewConf);
				int ConfigurationId = m_vConfigs.size() - 1;
				pCurrentConf = &m_vConfigs[ConfigurationId];
				str_copy(pCurrentConf->m_aName, pLine, minimum<int>(sizeof(pCurrentConf->m_aName), str_length(pLine)));

				// add start run
				CRun NewRun;
				NewRun.m_AutomapCopy = true;
				pCurrentConf->m_vRuns.push_back(NewRun);
				int RunId = pCurrentConf->m_vRuns.size() - 1;
				pCurrentRun = &pCurrentConf->m_vRuns[RunId];
			}
			else if(str_startswith(pLine, "NewRun") && pCurrentConf)
			{
				// add new run
				CRun NewRun;
				NewRun.m_AutomapCopy = true;
				pCurrentConf->m_vRuns.push_back(NewRun);
				int RunId = pCurrentConf->m_vRuns.size() - 1;
				pCurrentRun = &pCurrentConf->m_vRuns[RunId];
			}
			else if(str_startswith(pLine, "Index") && pCurrentRun)
			{
				// new index
				int Id = 0;
				char aOrientation1[128] = "";
				char aOrientation2[128] = "";
				char aOrientation3[128] = "";

				sscanf(pLine, "Index %d %127s %127s %127s", &Id, aOrientation1, aOrientation2, aOrientation3);

				CIndexRule NewIndexRule;
				NewIndexRule.m_Id = Id;
				NewIndexRule.m_Flag = 0;
				NewIndexRule.m_RandomProbability = 1.0f;
				NewIndexRule.m_DefaultRule = true;
				NewIndexRule.m_SkipEmpty = false;
				NewIndexRule.m_SkipFull = false;

				if(str_length(aOrientation1) > 0)
				{
					if(!str_comp(aOrientation1, "XFLIP"))
						NewIndexRule.m_Flag |= TILEFLAG_XFLIP;
					else if(!str_comp(aOrientation1, "YFLIP"))
						NewIndexRule.m_Flag |= TILEFLAG_YFLIP;
					else if(!str_comp(aOrientation1, "ROTATE"))
						NewIndexRule.m_Flag |= TILEFLAG_ROTATE;
				}

				if(str_length(aOrientation2) > 0)
				{
					if(!str_comp(aOrientation2, "XFLIP"))
						NewIndexRule.m_Flag |= TILEFLAG_XFLIP;
					else if(!str_comp(aOrientation2, "YFLIP"))
						NewIndexRule.m_Flag |= TILEFLAG_YFLIP;
					else if(!str_comp(aOrientation2, "ROTATE"))
						NewIndexRule.m_Flag |= TILEFLAG_ROTATE;
				}

				if(str_length(aOrientation3) > 0)
				{
					if(!str_comp(aOrientation3, "XFLIP"))
						NewIndexRule.m_Flag |= TILEFLAG_XFLIP;
					else if(!str_comp(aOrientation3, "YFLIP"))
						NewIndexRule.m_Flag |= TILEFLAG_YFLIP;
					else if(!str_comp(aOrientation3, "ROTATE"))
						NewIndexRule.m_Flag |= TILEFLAG_ROTATE;
				}

				// add the index rule object and make it current
				pCurrentRun->m_vIndexRules.push_back(NewIndexRule);
				int IndexRuleId = pCurrentRun->m_vIndexRules.size() - 1;
				pCurrentIndex = &pCurrentRun->m_vIndexRules[IndexRuleId];
			}
			else if(str_startswith(pLine, "Pos") && pCurrentIndex)
			{
				int x = 0, y = 0;
				char aValue[128];
				int Value = CPosRule::NORULE;
				std::vector<CIndexInfo> vNewIndexList;

				sscanf(pLine, "Pos %d %d %127s", &x, &y, aValue);

				if(!str_comp(aValue, "EMPTY"))
				{
					Value = CPosRule::INDEX;
					CIndexInfo NewIndexInfo = {0, 0, false};
					vNewIndexList.push_back(NewIndexInfo);
				}
				else if(!str_comp(aValue, "FULL"))
				{
					Value = CPosRule::NOTINDEX;
					CIndexInfo NewIndexInfo1 = {0, 0, false};
					// CIndexInfo NewIndexInfo2 = {-1, 0};
					vNewIndexList.push_back(NewIndexInfo1);
					// vNewIndexList.push_back(NewIndexInfo2);
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
						int Id = 0;
						char aOrientation1[128] = "";
						char aOrientation2[128] = "";
						char aOrientation3[128] = "";
						char aOrientation4[128] = "";
						sscanf(str_trim_words(pLine, pWord), "%d %127s %127s %127s %127s", &Id, aOrientation1, aOrientation2, aOrientation3, aOrientation4);

						CIndexInfo NewIndexInfo;
						NewIndexInfo.m_Id = Id;
						NewIndexInfo.m_Flag = 0;
						NewIndexInfo.m_TestFlag = false;

						if(!str_comp(aOrientation1, "OR"))
						{
							vNewIndexList.push_back(NewIndexInfo);
							pWord += 2;
							continue;
						}
						else if(str_length(aOrientation1) > 0)
						{
							NewIndexInfo.m_TestFlag = true;
							if(!str_comp(aOrientation1, "XFLIP"))
								NewIndexInfo.m_Flag = TILEFLAG_XFLIP;
							else if(!str_comp(aOrientation1, "YFLIP"))
								NewIndexInfo.m_Flag = TILEFLAG_YFLIP;
							else if(!str_comp(aOrientation1, "ROTATE"))
								NewIndexInfo.m_Flag = TILEFLAG_ROTATE;
							else if(!str_comp(aOrientation1, "NONE"))
								NewIndexInfo.m_Flag = 0;
							else
								NewIndexInfo.m_TestFlag = false;
						}
						else
						{
							vNewIndexList.push_back(NewIndexInfo);
							break;
						}

						if(!str_comp(aOrientation2, "OR"))
						{
							vNewIndexList.push_back(NewIndexInfo);
							pWord += 3;
							continue;
						}
						else if(str_length(aOrientation2) > 0 && NewIndexInfo.m_Flag != 0)
						{
							if(!str_comp(aOrientation2, "XFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_XFLIP;
							else if(!str_comp(aOrientation2, "YFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_YFLIP;
							else if(!str_comp(aOrientation2, "ROTATE"))
								NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
						}
						else
						{
							vNewIndexList.push_back(NewIndexInfo);
							break;
						}

						if(!str_comp(aOrientation3, "OR"))
						{
							vNewIndexList.push_back(NewIndexInfo);
							pWord += 4;
							continue;
						}
						else if(str_length(aOrientation3) > 0 && NewIndexInfo.m_Flag != 0)
						{
							if(!str_comp(aOrientation3, "XFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_XFLIP;
							else if(!str_comp(aOrientation3, "YFLIP"))
								NewIndexInfo.m_Flag |= TILEFLAG_YFLIP;
							else if(!str_comp(aOrientation3, "ROTATE"))
								NewIndexInfo.m_Flag |= TILEFLAG_ROTATE;
						}
						else
						{
							vNewIndexList.push_back(NewIndexInfo);
							break;
						}

						if(!str_comp(aOrientation4, "OR"))
						{
							vNewIndexList.push_back(NewIndexInfo);
							pWord += 5;
							continue;
						}
						else
						{
							vNewIndexList.push_back(NewIndexInfo);
							break;
						}
					}
				}

				if(Value != CPosRule::NORULE)
				{
					CPosRule NewPosRule = {x, y, Value, vNewIndexList};
					pCurrentIndex->m_vRules.push_back(NewPosRule);

					pCurrentConf->m_StartX = minimum(pCurrentConf->m_StartX, NewPosRule.m_X);
					pCurrentConf->m_StartY = minimum(pCurrentConf->m_StartY, NewPosRule.m_Y);
					pCurrentConf->m_EndX = maximum(pCurrentConf->m_EndX, NewPosRule.m_X);
					pCurrentConf->m_EndY = maximum(pCurrentConf->m_EndY, NewPosRule.m_Y);

					if(x == 0 && y == 0)
					{
						for(const auto &Index : vNewIndexList)
						{
							if(Index.m_Id == 0 && Value == CPosRule::INDEX)
							{
								// Skip full tiles if we have a rule "POS 0 0 INDEX 0"
								// because that forces the tile to be empty
								pCurrentIndex->m_SkipFull = true;
							}
							else if((Index.m_Id > 0 && Value == CPosRule::INDEX) || (Index.m_Id == 0 && Value == CPosRule::NOTINDEX))
							{
								// Skip empty tiles if we have a rule "POS 0 0 INDEX i" where i > 0
								// or if we have a rule "POS 0 0 NOTINDEX 0"
								pCurrentIndex->m_SkipEmpty = true;
							}
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
	for(auto &Config : m_vConfigs)
	{
		for(auto &Run : Config.m_vRuns)
		{
			for(auto &IndexRule : Run.m_vIndexRules)
			{
				bool Found = false;

				// Search for the exact rule "POS 0 0 INDEX 0" which corresponds to the default rule
				for(const auto &Rule : IndexRule.m_vRules)
				{
					if(Rule.m_X == 0 && Rule.m_Y == 0 && Rule.m_Value == CPosRule::INDEX)
					{
						for(const auto &Index : Rule.m_vIndexList)
						{
							if(Index.m_Id == 0)
								Found = true;
						}
						break;
					}

					if(Found)
						break;
				}

				// If the default rule was not found, and we require it, then add it
				if(!Found && IndexRule.m_DefaultRule)
				{
					std::vector<CIndexInfo> vNewIndexList;
					CIndexInfo NewIndexInfo = {0, 0, false};
					vNewIndexList.push_back(NewIndexInfo);
					CPosRule NewPosRule = {0, 0, CPosRule::NOTINDEX, vNewIndexList};
					IndexRule.m_vRules.push_back(NewPosRule);

					IndexRule.m_SkipEmpty = true;
					IndexRule.m_SkipFull = false;
				}

				if(IndexRule.m_SkipEmpty && IndexRule.m_SkipFull)
				{
					IndexRule.m_SkipEmpty = false;
					IndexRule.m_SkipFull = false;
				}
			}
		}
	}

	char aBuf[IO_MAX_PATH_LENGTH + 16];
	str_format(aBuf, sizeof(aBuf), "loaded %s", aPath);
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "editor/automap", aBuf);

	m_FileLoaded = true;
}

const char *CAutoMapper::GetConfigName(int Index)
{
	if(Index < 0 || Index >= (int)m_vConfigs.size())
		return "";

	return m_vConfigs[Index].m_aName;
}

void CAutoMapper::ProceedLocalized(CLayerTiles *pLayer, int ConfigId, int Seed, int X, int Y, int Width, int Height)
{
	if(!m_FileLoaded || pLayer->m_Readonly || ConfigId < 0 || ConfigId >= (int)m_vConfigs.size())
		return;

	if(Width < 0)
		Width = pLayer->m_Width;

	if(Height < 0)
		Height = pLayer->m_Height;

	CConfiguration *pConf = &m_vConfigs[ConfigId];

	int CommitFromX = clamp(X + pConf->m_StartX, 0, pLayer->m_Width);
	int CommitFromY = clamp(Y + pConf->m_StartY, 0, pLayer->m_Height);
	int CommitToX = clamp(X + Width + pConf->m_EndX, 0, pLayer->m_Width);
	int CommitToY = clamp(Y + Height + pConf->m_EndY, 0, pLayer->m_Height);

	int UpdateFromX = clamp(X + 3 * pConf->m_StartX, 0, pLayer->m_Width);
	int UpdateFromY = clamp(Y + 3 * pConf->m_StartY, 0, pLayer->m_Height);
	int UpdateToX = clamp(X + Width + 3 * pConf->m_EndX, 0, pLayer->m_Width);
	int UpdateToY = clamp(Y + Height + 3 * pConf->m_EndY, 0, pLayer->m_Height);

	CLayerTiles *pUpdateLayer = new CLayerTiles(Editor(), UpdateToX - UpdateFromX, UpdateToY - UpdateFromY);

	for(int y = UpdateFromY; y < UpdateToY; y++)
	{
		for(int x = UpdateFromX; x < UpdateToX; x++)
		{
			CTile *pIn = &pLayer->m_pTiles[y * pLayer->m_Width + x];
			CTile *pOut = &pUpdateLayer->m_pTiles[(y - UpdateFromY) * pUpdateLayer->m_Width + x - UpdateFromX];
			pOut->m_Index = pIn->m_Index;
			pOut->m_Flags = pIn->m_Flags;
		}
	}

	Proceed(pUpdateLayer, ConfigId, Seed, UpdateFromX, UpdateFromY);

	for(int y = CommitFromY; y < CommitToY; y++)
	{
		for(int x = CommitFromX; x < CommitToX; x++)
		{
			CTile *pIn = &pUpdateLayer->m_pTiles[(y - UpdateFromY) * pUpdateLayer->m_Width + x - UpdateFromX];
			CTile *pOut = &pLayer->m_pTiles[y * pLayer->m_Width + x];
			CTile Previous = *pOut;
			pOut->m_Index = pIn->m_Index;
			pOut->m_Flags = pIn->m_Flags;
			pLayer->RecordStateChange(x, y, Previous, *pOut);
		}
	}

	delete pUpdateLayer;
}

void CAutoMapper::Proceed(CLayerTiles *pLayer, int ConfigId, int Seed, int SeedOffsetX, int SeedOffsetY)
{
	if(!m_FileLoaded || pLayer->m_Readonly || ConfigId < 0 || ConfigId >= (int)m_vConfigs.size())
		return;

	if(Seed == 0)
		Seed = rand();

	CConfiguration *pConf = &m_vConfigs[ConfigId];
	pLayer->ClearHistory();

	// for every run: copy tiles, automap, overwrite tiles
	for(size_t h = 0; h < pConf->m_vRuns.size(); ++h)
	{
		CRun *pRun = &pConf->m_vRuns[h];

		// don't make copy if it's requested
		CLayerTiles *pReadLayer;
		if(pRun->m_AutomapCopy)
		{
			pReadLayer = new CLayerTiles(Editor(), pLayer->m_Width, pLayer->m_Height);

			for(int y = 0; y < pLayer->m_Height; y++)
			{
				for(int x = 0; x < pLayer->m_Width; x++)
				{
					CTile *pIn = &pLayer->m_pTiles[y * pLayer->m_Width + x];
					CTile *pOut = &pReadLayer->m_pTiles[y * pLayer->m_Width + x];
					pOut->m_Index = pIn->m_Index;
					pOut->m_Flags = pIn->m_Flags;
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
				const CTile *pReadTile = &(pReadLayer->m_pTiles[y * pLayer->m_Width + x]);
				Editor()->m_Map.OnModify();

				for(size_t i = 0; i < pRun->m_vIndexRules.size(); ++i)
				{
					CIndexRule *pIndexRule = &pRun->m_vIndexRules[i];
					if(pIndexRule->m_SkipEmpty && pReadTile->m_Index == 0) // skip empty tiles
						continue;
					if(pIndexRule->m_SkipFull && pReadTile->m_Index != 0) // skip full tiles
						continue;

					bool RespectRules = true;
					for(size_t j = 0; j < pIndexRule->m_vRules.size() && RespectRules; ++j)
					{
						CPosRule *pRule = &pIndexRule->m_vRules[j];

						int CheckIndex, CheckFlags;
						int CheckX = x + pRule->m_X;
						int CheckY = y + pRule->m_Y;
						if(CheckX >= 0 && CheckX < pLayer->m_Width && CheckY >= 0 && CheckY < pLayer->m_Height)
						{
							int CheckTile = CheckY * pLayer->m_Width + CheckX;
							CheckIndex = pReadLayer->m_pTiles[CheckTile].m_Index;
							CheckFlags = pReadLayer->m_pTiles[CheckTile].m_Flags & (TILEFLAG_ROTATE | TILEFLAG_XFLIP | TILEFLAG_YFLIP);
						}
						else
						{
							CheckIndex = -1;
							CheckFlags = 0;
						}

						if(pRule->m_Value == CPosRule::INDEX)
						{
							RespectRules = false;
							for(const auto &Index : pRule->m_vIndexList)
							{
								if(CheckIndex == Index.m_Id && (!Index.m_TestFlag || CheckFlags == Index.m_Flag))
								{
									RespectRules = true;
									break;
								}
							}
						}
						else if(pRule->m_Value == CPosRule::NOTINDEX)
						{
							for(const auto &Index : pRule->m_vIndexList)
							{
								if(CheckIndex == Index.m_Id && (!Index.m_TestFlag || CheckFlags == Index.m_Flag))
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
						CTile Previous = *pTile;
						pTile->m_Index = pIndexRule->m_Id;
						pTile->m_Flags = pIndexRule->m_Flag;
						pLayer->RecordStateChange(x, y, Previous, *pTile);
					}
				}
			}
		}

		// clean-up
		if(pRun->m_AutomapCopy && pReadLayer != pLayer)
			delete pReadLayer;
	}
}
