#ifndef GAME_EDITOR_AUTO_MAP_H
#define GAME_EDITOR_AUTO_MAP_H

#include <base/tl/array.h>

class CAutoMapper
{
	struct CIndexInfo
	{
		int m_ID;
		int m_Flag;
		bool m_TestFlag;
	};

	struct CPosRule
	{
		int m_X;
		int m_Y;
		int m_Value;
		array<CIndexInfo> m_aIndexList;

		enum
		{
			NORULE = 0,
			INDEX,
			NOTINDEX
		};
	};

	struct CIndexRule
	{
		int m_ID;
		array<CPosRule> m_aRules;
		int m_Flag;
		float m_RandomProbability;
		bool m_DefaultRule;
		bool m_SkipEmpty;
		bool m_SkipFull;
	};

	struct CRun
	{
		array<CIndexRule> m_aIndexRules;
		bool m_AutomapCopy;
	};

	struct CConfiguration
	{
		array<CRun> m_aRuns;
		char m_aName[128];
		int m_StartX;
		int m_StartY;
		int m_EndX;
		int m_EndY;
	};

public:
	CAutoMapper(class CEditor *pEditor);

	void Load(const char *pTileName);
	void ProceedLocalized(class CLayerTiles *pLayer, int ConfigID, int Seed = 0, int X = 0, int Y = 0, int Width = -1, int Height = -1);
	void Proceed(class CLayerTiles *pLayer, int ConfigID, int Seed = 0, int SeedOffsetX = 0, int SeedOffsetY = 0);

	int ConfigNamesNum() const { return m_lConfigs.size(); }
	const char *GetConfigName(int Index);

	bool IsLoaded() const { return m_FileLoaded; }

private:
	array<CConfiguration> m_lConfigs;
	class CEditor *m_pEditor;
	bool m_FileLoaded;
};

#endif
