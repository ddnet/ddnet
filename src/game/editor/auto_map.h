#ifndef GAME_EDITOR_AUTO_MAP_H
#define GAME_EDITOR_AUTO_MAP_H

#include <vector>

class CAutoMapper
{
	struct CIndexInfo
	{
		int m_ID = 0;
		int m_Flag = 0;
		bool m_TestFlag = false;
	};

	struct CPosRule
	{
		int m_X = 0;
		int m_Y = 0;
		int m_Value = 0;
		std::vector<CIndexInfo> m_vIndexList;

		enum
		{
			NORULE = 0,
			INDEX,
			NOTINDEX
		};
	};

	struct CIndexRule
	{
		int m_ID = 0;
		std::vector<CPosRule> m_vRules;
		int m_Flag = 0;
		float m_RandomProbability = 0;
		bool m_DefaultRule = false;
		bool m_SkipEmpty = false;
		bool m_SkipFull = false;
	};

	struct CRun
	{
		std::vector<CIndexRule> m_vIndexRules;
		bool m_AutomapCopy = false;
	};

	struct CConfiguration
	{
		std::vector<CRun> m_vRuns;
		char m_aName[128] = {0};
		int m_StartX = 0;
		int m_StartY = 0;
		int m_EndX = 0;
		int m_EndY = 0;
	};

public:
	CAutoMapper(class CEditor *pEditor);

	void Load(const char *pTileName);
	void ProceedLocalized(class CLayerTiles *pLayer, int ConfigID, int Seed = 0, int X = 0, int Y = 0, int Width = -1, int Height = -1);
	void Proceed(class CLayerTiles *pLayer, int ConfigID, int Seed = 0, int SeedOffsetX = 0, int SeedOffsetY = 0);

	int ConfigNamesNum() const { return m_vConfigs.size(); }
	const char *GetConfigName(int Index);

	bool IsLoaded() const { return m_FileLoaded; }

private:
	std::vector<CConfiguration> m_vConfigs;
	class CEditor *m_pEditor = nullptr;
	bool m_FileLoaded = false;
};

#endif
