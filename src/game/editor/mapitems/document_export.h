#ifndef GAME_EDITOR_MAPITEMS_DOCUMENT_EXPORT_H
#define GAME_EDITOR_MAPITEMS_DOCUMENT_EXPORT_H

#include "document_graph.h"

class CDataFileWriter;

using CEditorTilePayload = std::variant<
	const CEditorTilePlane<CTileValues> *,
	const CEditorTilePlane<CTeleTileValues> *,
	const CEditorTilePlane<CSpeedupTileValues> *,
	const CEditorTilePlane<CSwitchTileValues> *,
	const CEditorTilePlane<CTuneTileValues> *>;

/**
 * A single format projection feeds both map writing and content fingerprints.
 * Persisted fields belong in ExportEditorDocument; history-only authoring values
 * deliberately do not affect this projection or the saved-content marker.
 */
class IEditorDocumentOutput
{
public:
	virtual ~IEditorDocumentOutput() = default;
	virtual int AddItem(int Type, int Id, std::size_t Size, const void *pData) = 0;
	virtual int AddData(std::size_t Size, const void *pData) = 0;
	virtual int AddDataSwapped(std::size_t Size, const void *pData) = 0;
	virtual int AddResourceBlob(const editor_history::CResourceBlob &Blob);
	virtual int AddTilePlane(CEditorTilePayload Plane, const std::array<unsigned char, 256> &aFlags = {});
	virtual int AddEmptyTiles(std::size_t Count);
	virtual int AddLayerObjects(const editor_history::CSharedValue<CLayerDocumentValues> &Layer, const CEditorDocumentValues &Document);
	virtual std::array<unsigned char, 256> ImageTileFlags(const CEditorImageValues &Image);
	int AddDataString(const char *pString)
	{
		return pString[0] == '\0' ? -1 : AddData(str_length(pString) + 1, pString);
	}
};

/** Pure projection, including normalized compatibility data and active tangents. */
bool ExportEditorDocument(const CEditorDocumentValues &Document, IEditorDocumentOutput &Writer, std::string &Error);
/** Rejects unavailable embedded resources, then copies pure output to the writer. */
bool WriteEditorDocument(const CEditorDocumentValues &Document, CDataFileWriter &Writer, std::string &Error);
/** Derived cache belongs to the editor runtime, never to a document revision. */
class CEditorDocumentFingerprint
{
	class CCache;
	std::unique_ptr<CCache> m_pCache;
	std::uint64_t m_Generation = 0;

public:
	struct CStatistics
	{
		std::size_t m_BlobScans = 0;
		std::size_t m_TileChunks = 0; // Logical changed chunks, including implicit empty chunks.
		std::size_t m_TileLeafHashes = 0;
		std::size_t m_TileBranchHashes = 0;
		std::size_t m_ImagesAnalyzed = 0;
		std::size_t m_ObjectBlocks = 0;
	};
	enum class EHashBackend
	{
		PLATFORM,
		PORTABLE
	};
	CEditorDocumentFingerprint();
	explicit CEditorDocumentFingerprint(EHashBackend Backend);
	~CEditorDocumentFingerprint();
	std::string Key(const CEditorDocumentValues &Document);
	const CStatistics &Statistics() const;
	void Account(editor_history::CStorageUsage &Usage) const;
	void Observe(editor_history::CStorageObservation &Observation) const;
	std::uint64_t Generation() const { return m_Generation; }
	void Clear();
};

/** Does not require resources to be available, allowing dirty checks of editable maps. */
std::string EditorDocumentPersistedKey(const CEditorDocumentValues &Document);

#endif
