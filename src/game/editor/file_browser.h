/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_EDITOR_FILE_BROWSER_H
#define GAME_EDITOR_FILE_BROWSER_H

#include "component.h"

#include <base/types.h>

#include <game/client/ui.h>
#include <game/client/ui_listbox.h>

#include <optional>
#include <vector>

class CFileBrowser : public CEditorComponent
{
public:
	enum class EFileType
	{
		MAP,
		IMAGE,
		SOUND,
	};
	typedef bool (*FFileDialogOpenCallback)(const char *pFilename, int StorageType, void *pUser);

	void ShowFileDialog(
		int StorageType, EFileType FileType,
		const char *pTitle, const char *pButtonText,
		const char *pInitialPath, const char *pInitialFilename,
		FFileDialogOpenCallback pfnOpenCallback, void *pOpenCallbackUser);
	void OnRender(CUIRect _) override;
	bool IsValidSaveFilename() const;

	void OnEditorClose();
	void OnDialogClose();

private:
	/**
	 * Storage type for which the file browser is currently showing entries.
	 */
	int m_StorageType = 0;
	/**
	 * File type for which the file browser is currently showing entries.
	 */
	EFileType m_FileType = EFileType::MAP;
	/**
	 * `true` if file browser was opened with the intent to save a file.
	 * `false` if file browser was opened with the intent to open an existing file for reading.
	 */
	bool m_SaveAction = false;
	/**
	 * Whether multiple storage locations with the initial folder are available.
	 */
	bool m_MultipleStorages = false;
	/**
	 * Title text of the file dialog.
	 */
	char m_aTitle[128] = "";
	/**
	 * Text of the confirmation button that opens/saves the file.
	 */
	char m_aButtonText[64] = "";
	/**
	 * Callback function that will be called when the confirmation button is pressed.
	 */
	FFileDialogOpenCallback m_pfnOpenCallback = nullptr;
	/**
	 * User data for @link m_pfnOpenCallback @endlink.
	 */
	void *m_pOpenCallbackUser = nullptr;
	/**
	 * Whether the list of storages is currently being shown, if @link m_MultipleStorages @endlink is `true`.
	 */
	bool m_ShowingRoot = false;
	/**
	 * Path of the initial folder that the file browser was opened with.
	 */
	char m_aInitialFolder[IO_MAX_PATH_LENGTH] = "";
	/**
	 * Path of the current folder being shown in the file browser.
	 */
	char m_aCurrentFolder[IO_MAX_PATH_LENGTH] = "";
	/**
	 * Path of the current link being shown in the file browser.
	 */
	char m_aCurrentLink[IO_MAX_PATH_LENGTH] = "";
	/**
	 * Current path being shown in the file browser, which either points to @link m_aCurrentFolder @endlink or @link m_aCurrentLink @endlink.
	 */
	char *m_pCurrentPath = m_aCurrentFolder;
	/**
	 * Input for the filename when saving files and also buffer for the selected file's name in general.
	 */
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_FilenameInput;
	/**
	 * Input for the file search when opening files for reading.
	 */
	CLineInputBuffered<IO_MAX_PATH_LENGTH> m_FilterInput;
	/**
	 * Index of the selected file list entry in @link m_vpFilteredFileList @endlink.
	 */
	int m_SelectedFileIndex = -1;
	/**
	 * Display name of the selected file list entry in @link m_vpFilteredFileList @endlink and @link m_vCompleteFileList @endlink.
	 */
	char m_aSelectedFileDisplayName[IO_MAX_PATH_LENGTH] = "";

	// File list
	class CFilelistItem
	{
	public:
		char m_aFilename[IO_MAX_PATH_LENGTH];
		char m_aDisplayName[IO_MAX_PATH_LENGTH];
		bool m_IsDir;
		bool m_IsLink;
		int m_StorageType;
		time_t m_TimeModified;
	};
	std::vector<CFilelistItem> m_vCompleteFileList;
	std::vector<const CFilelistItem *> m_vpFilteredFileList;
	enum class ESortDirection
	{
		NEUTRAL,
		ASCENDING,
		DESCENDING,
	};
	ESortDirection m_SortByFilename = ESortDirection::ASCENDING;
	ESortDirection m_SortByTimeModified = ESortDirection::NEUTRAL;

	// File preview
	enum class EPreviewState
	{
		UNLOADED,
		LOADED,
		ERROR,
	};
	EPreviewState m_PreviewState = EPreviewState::UNLOADED;
	IGraphics::CTextureHandle m_PreviewImage;
	int m_PreviewImageWidth = 0;
	int m_PreviewImageHeight = 0;
	int m_PreviewSound = -1;

	// UI elements
	CListBox m_ListBox;
	const char m_ButtonSortTimeModifiedId = 0;
	const char m_ButtonSortFilenameId = 0;
	const char m_ButtonPlayPauseId = 0;
	const char m_ButtonStopId = 0;
	const char m_SeekBarId = 0;
	const char m_ButtonOkId = 0;
	const char m_ButtonCancelId = 0;
	const char m_ButtonRefreshId = 0;
	const char m_ButtonShowDirectoryId = 0;
	const char m_ButtonDeleteId = 0;
	const char m_ButtonNewFolderId = 0;

	bool CanPreviewFile() const;
	void UpdateFilePreview();
	void RenderFilePreview(CUIRect Preview);
	const char *DetermineFileFontIcon(const CFilelistItem *pItem) const;
	void UpdateFilenameInput();
	void UpdateSelectedIndex(const char *pDisplayName);
	void SortFilteredFileList();
	void RefreshFilteredFileList();
	void FilelistPopulate(int StorageType, bool KeepSelection);
	static int DirectoryListingCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser);
	static std::optional<bool> CompareCommon(const CFilelistItem *pLhs, const CFilelistItem *pRhs);
	static bool CompareFilenameAscending(const CFilelistItem *pLhs, const CFilelistItem *pRhs);
	static bool CompareFilenameDescending(const CFilelistItem *pLhs, const CFilelistItem *pRhs);
	static bool CompareTimeModifiedAscending(const CFilelistItem *pLhs, const CFilelistItem *pRhs);
	static bool CompareTimeModifiedDescending(const CFilelistItem *pLhs, const CFilelistItem *pRhs);

	class CPopupNewFolder : public SPopupMenuId
	{
	public:
		CFileBrowser *m_pFileBrowser;
		CLineInputBuffered<IO_MAX_PATH_LENGTH> m_NewFolderNameInput;
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		const char m_ButtonCancelId = 0;
		const char m_ButtonCreateId = 0;
	};
	CPopupNewFolder m_PopupNewFolder;

	class CPopupConfirmDelete : public SPopupMenuId
	{
	public:
		CFileBrowser *m_pFileBrowser;
		bool m_IsDirectory;
		char m_aDeletePath[IO_MAX_PATH_LENGTH];
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		const char m_ButtonCancelId = 0;
		const char m_ButtonDeleteId = 0;
	};
	CPopupConfirmDelete m_PopupConfirmDelete;

	class CPopupConfirmOverwrite : public SPopupMenuId
	{
	public:
		CFileBrowser *m_pFileBrowser;
		char m_aOverwritePath[IO_MAX_PATH_LENGTH];
		static CUi::EPopupMenuFunctionResult Render(void *pContext, CUIRect View, bool Active);

	private:
		const char m_ButtonCancelId = 0;
		const char m_ButtonOverwriteId = 0;
	};
	CPopupConfirmOverwrite m_PopupConfirmOverwrite;
};

#endif
