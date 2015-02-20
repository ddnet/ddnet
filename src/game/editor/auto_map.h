#ifndef GAME_EDITOR_AUTO_MAP_H
#define GAME_EDITOR_AUTO_MAP_H

#include <base/tl/array.h>

class CAutoMapper
{
	struct CIndexInfo
	{
		int m_ID;
		int m_Flag;
	};

	struct CPosRule
	{
		int m_X;
		int m_Y;
		int m_Value;
		array<CIndexInfo> m_aIndexList;

		enum
		{
			NORULE=0,
			EMPTY,
			FULL,
			INDEX,
			NOTINDEX
		};
	};

	struct CIndexRule
	{
		int m_ID;
		array<CPosRule> m_aRules;
		int m_Flag;
		int m_RandomValue;
		bool m_DefaultRule;
	};

	struct CRun
	{
		array<CIndexRule> m_aIndexRules;
	};

	struct CConfiguration
	{
		array<CRun> m_aRuns;
		char m_aName[128];
	};

public:
	CAutoMapper(class CEditor *pEditor);

	void Load(const char* pTileName);
	void Proceed(class CLayerTiles *pLayer, int ConfigID);

	int ConfigNamesNum() { return m_lConfigs.size(); }
	const char* GetConfigName(int Index);

	const bool IsLoaded() { return m_FileLoaded; }
private:
	array<CConfiguration> m_lConfigs;
	class CEditor *m_pEditor;
	bool m_FileLoaded;
};


#endif
