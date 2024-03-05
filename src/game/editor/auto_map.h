#ifndef GAME_EDITOR_AUTO_MAP_H
#define GAME_EDITOR_AUTO_MAP_H

#include <vector>

#include "component.h"

class CAutoMapper : public CEditorComponent
{
	struct CIndexInfo
	{
		int m_Id;
		int m_Flag;
		bool m_TestFlag;
	};

	struct CPosRule
	{
		int m_X;
		int m_Y;
		int m_Value;
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
		int m_Id;
		std::vector<CPosRule> m_vRules;
		int m_Flag;
		float m_RandomProbability;
		bool m_DefaultRule;
		bool m_SkipEmpty;
		bool m_SkipFull;
	};

	struct CRun
	{
		std::vector<CIndexRule> m_vIndexRules;
		bool m_AutomapCopy;
	};

	struct CConfiguration
	{
		std::vector<CRun> m_vRuns;
		char m_aName[128];
		int m_StartX;
		int m_StartY;
		int m_EndX;
		int m_EndY;
	};

public:
	explicit CAutoMapper(CEditor *pEditor);

	void Load(const char *pTileName);
	void ProceedLocalized(class CLayerTiles *pLayer, int ConfigId, int Seed = 0, int X = 0, int Y = 0, int Width = -1, int Height = -1);
	void Proceed(class CLayerTiles *pLayer, int ConfigId, int Seed = 0, int SeedOffsetX = 0, int SeedOffsetY = 0);

	int ConfigNamesNum() const { return m_vConfigs.size(); }
	const char *GetConfigName(int Index);

	bool IsLoaded() const { return m_FileLoaded; }

private:
	std::vector<CConfiguration> m_vConfigs = {};
	bool m_FileLoaded = false;
};

#endif
