#ifndef GAME_EDITOR_MAPITEMS_DOCUMENT_SESSION_H
#define GAME_EDITOR_MAPITEMS_DOCUMENT_SESSION_H

#include "document_values.h"

#include <unordered_map>
#include <unordered_set>

/** Session records are keyed by identity and are never part of a revision. */
class CGroupSessionValues
{
public:
	bool m_Visible = true;
	bool m_Collapse = false;
};

class CLayerSessionValues
{
public:
	bool m_Visible = true;
	bool m_Readonly = false;
	CDocumentText<IO_MAX_PATH_LENGTH> m_Filename;
	bool m_KnownTextModeLayer = false;
	bool m_RenderOverlays = true;
	unsigned char m_TeleNumber = 0;
	unsigned char m_TeleCheckpointNumber = 0;
	int m_GotoTeleOffset = 0;
	ivec2 m_GotoTeleLastPos{-1, -1};
	int m_SpeedupForce = 0;
	int m_SpeedupMaxSpeed = 0;
	int m_SpeedupAngle = 0;
	unsigned char m_SwitchNumber = 0;
	unsigned char m_SwitchDelay = 0;
	int m_GotoSwitchOffset = 0;
	ivec2 m_GotoSwitchLastPos{-1, -1};
	unsigned char m_TuningNumber = 0;
	int m_GotoTuneOffset = 0;
	ivec2 m_GotoTuneLastPos{-1, -1};
};

class CDocumentSessionValues
{
public:
	std::unordered_map<std::uint64_t, CGroupSessionValues> m_Groups;
	std::unordered_map<std::uint64_t, CLayerSessionValues> m_Layers;

	void Prune(const std::unordered_set<std::uint64_t> &RetainedIds)
	{
		std::erase_if(m_Groups, [&](const auto &Entry) { return !RetainedIds.contains(Entry.first); });
		std::erase_if(m_Layers, [&](const auto &Entry) { return !RetainedIds.contains(Entry.first); });
	}
};

/** Resolve a retained selection against today's ordering, never a pointer. */
template<typename T, typename F>
int DocumentIndexById(const std::vector<T> &vValues, std::uint64_t Id, F Identity)
{
	if(Id == 0)
		return -1;
	for(std::size_t Index = 0; Index < vValues.size(); ++Index)
		if(Identity(vValues[Index]) == Id)
			return static_cast<int>(Index);
	return -1;
}

#endif
