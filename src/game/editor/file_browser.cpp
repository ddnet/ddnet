/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "file_browser.h"

#include <engine/keys.h>
#include <engine/sound.h>
#include <engine/storage.h>

#include <game/editor/editor.h>

using namespace FontIcons;

static constexpr const char *FILETYPE_EXTENSIONS[] = {
	".map",
	".png",
	".opus"};

void CFileBrowser::ShowFileDialog(
	int StorageType, EFileType FileType,
	const char *pTitle, const char *pButtonText,
	const char *pInitialPath, const char *pInitialFilename,
	FFileDialogOpenCallback pfnOpenCallback, void *pOpenCallbackUser)
{
	m_StorageType = StorageType;
	m_FileType = FileType;
	if(m_StorageType == IStorage::TYPE_ALL)
	{
		int NumStoragesWithFolder = 0;
		for(int CheckStorageType = IStorage::TYPE_SAVE; CheckStorageType < Storage()->NumPaths(); ++CheckStorageType)
		{
			if(Storage()->FolderExists(pInitialPath, CheckStorageType))
			{
				NumStoragesWithFolder++;
			}
		}
		m_MultipleStorages = NumStoragesWithFolder > 1;
	}
	else
	{
		m_MultipleStorages = false;
	}
	m_SaveAction = m_StorageType == IStorage::TYPE_SAVE;

	Ui()->ClosePopupMenus();
	str_copy(m_aTitle, pTitle);
	str_copy(m_aButtonText, pButtonText);
	m_pfnOpenCallback = pfnOpenCallback;
	m_pOpenCallbackUser = pOpenCallbackUser;
	m_ShowingRoot = false;
	str_copy(m_aInitialFolder, pInitialPath);
	str_copy(m_aCurrentFolder, pInitialPath);
	m_aCurrentLink[0] = '\0';
	m_pCurrentPath = m_aCurrentFolder;
	m_FilenameInput.Set(pInitialFilename);
	m_FilterInput.Clear();
	dbg_assert(m_PreviewState == EPreviewState::UNLOADED, "Preview was not unloaded before showing file dialog");
	m_ListBox.Reset();

	FilelistPopulate(m_StorageType, false);

	if(m_SaveAction)
	{
		Ui()->SetActiveItem(&m_FilenameInput);
		if(!m_FilenameInput.IsEmpty())
		{
			UpdateSelectedIndex(m_FilenameInput.GetString());
		}
	}
	else
	{
		Ui()->SetActiveItem(&m_FilterInput);
		UpdateFilenameInput();
	}

	Editor()->m_Dialog = DIALOG_FILE;
}

void CFileBrowser::OnRender(CUIRect _)
{
	if(Editor()->m_Dialog != DIALOG_FILE)
	{
		return;
	}

	Ui()->MapScreen();
	CUIRect View = *Ui()->Screen();
	CUIRect Preview = {0.0f, 0.0f, 0.0f, 0.0f};
	const float OriginalWidth = View.w;
	const float OriginalHeight = View.h;

	// Prevent UI elements below the file browser from being activated.
	Ui()->SetHotItem(this);

	View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_NONE, 0.0f);
	View.VMargin(150.0f, &View);
	View.HMargin(50.0f, &View);
	View.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.75f), IGraphics::CORNER_ALL, 5.0f);
	View.Margin(10.0f, &View);

	CUIRect Title, FileBox, FileBoxLabel, ButtonBar, PathBox;
	View.HSplitTop(20.0f, &Title, &View);
	View.HSplitTop(5.0f, nullptr, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);
	View.HSplitBottom(5.0f, &View, nullptr);
	View.HSplitBottom(15.0f, &View, &PathBox);
	View.HSplitBottom(5.0f, &View, nullptr);
	View.HSplitBottom(15.0f, &View, &FileBox);
	FileBox.VSplitLeft(55.0f, &FileBoxLabel, &FileBox);
	View.HSplitBottom(5.0f, &View, nullptr);
	if(CanPreviewFile())
	{
		View.VSplitMid(&View, &Preview);
	}

	// Title bar sort buttons
	if(!m_ShowingRoot)
	{
		CUIRect ButtonTimeModified, ButtonFilename;
		Title.VSplitRight(m_ListBox.ScrollbarWidthMax(), &Title, nullptr);
		Title.VSplitRight(90.0f, &Title, &ButtonTimeModified);
		Title.VSplitRight(5.0f, &Title, nullptr);
		Title.VSplitRight(90.0f, &Title, &ButtonFilename);
		Title.VSplitRight(5.0f, &Title, nullptr);

		static constexpr const char *SORT_INDICATORS[] = {"", "▲", "▼"};

		char aLabelButtonSortTimeModified[64];
		str_format(aLabelButtonSortTimeModified, sizeof(aLabelButtonSortTimeModified), "Time modified %s", SORT_INDICATORS[(int)m_SortByTimeModified]);
		if(Editor()->DoButton_Editor(&m_ButtonSortTimeModifiedId, aLabelButtonSortTimeModified, 0, &ButtonTimeModified, BUTTONFLAG_LEFT, "Sort by time modified."))
		{
			if(m_SortByTimeModified == ESortDirection::ASCENDING)
			{
				m_SortByTimeModified = ESortDirection::DESCENDING;
			}
			else if(m_SortByTimeModified == ESortDirection::DESCENDING)
			{
				m_SortByTimeModified = ESortDirection::NEUTRAL;
			}
			else
			{
				m_SortByTimeModified = ESortDirection::ASCENDING;
			}

			RefreshFilteredFileList();
		}

		char aLabelButtonSortFilename[64];
		str_format(aLabelButtonSortFilename, sizeof(aLabelButtonSortFilename), "Filename %s", SORT_INDICATORS[(int)m_SortByFilename]);
		if(Editor()->DoButton_Editor(&m_ButtonSortFilenameId, aLabelButtonSortFilename, 0, &ButtonFilename, BUTTONFLAG_LEFT, "Sort by filename."))
		{
			if(m_SortByFilename == ESortDirection::DESCENDING)
			{
				m_SortByFilename = ESortDirection::ASCENDING;
				m_SortByTimeModified = ESortDirection::NEUTRAL;
			}
			else
			{
				m_SortByFilename = ESortDirection::DESCENDING;
				m_SortByTimeModified = ESortDirection::NEUTRAL;
			}

			RefreshFilteredFileList();
		}
	}

	// Title
	Title.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 5.0f);
	Title.VMargin(10.0f, &Title);
	Ui()->DoLabel(&Title, m_aTitle, 12.0f, TEXTALIGN_ML);

	// Current path
	if(m_SelectedFileIndex >= 0 && m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType >= IStorage::TYPE_SAVE)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType, m_pCurrentPath, aPath, sizeof(aPath));
		char aPathLabel[128 + IO_MAX_PATH_LENGTH];
		str_format(aPathLabel, sizeof(aPathLabel), "Current path: %s", aPath);
		Ui()->DoLabel(&PathBox, aPathLabel, 10.0f, TEXTALIGN_ML, {.m_MaxWidth = PathBox.w, .m_EllipsisAtEnd = true});
	}

	m_ListBox.SetActive(!Ui()->IsPopupOpen());

	// Filename/filter input
	if(m_SaveAction)
	{
		// Filename input when saving
		Ui()->DoLabel(&FileBoxLabel, "Filename:", 10.0f, TEXTALIGN_ML);
		if(Ui()->DoEditBox(&m_FilenameInput, &FileBox, 10.0f))
		{
			// Remove '/' and '\'
			for(int i = 0; m_FilenameInput.GetString()[i]; ++i)
			{
				if(m_FilenameInput.GetString()[i] == '/' || m_FilenameInput.GetString()[i] == '\\')
				{
					m_FilenameInput.SetRange(m_FilenameInput.GetString() + i + 1, i, m_FilenameInput.GetLength());
					--i;
				}
			}
			UpdateSelectedIndex(m_FilenameInput.GetString());
		}
	}
	else
	{
		// Filter input when loading
		Ui()->DoLabel(&FileBoxLabel, "Search:", 10.0f, TEXTALIGN_ML);
		if(Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed())
		{
			Ui()->SetActiveItem(&m_FilterInput);
			m_FilterInput.SelectAll();
		}
		if(Ui()->DoClearableEditBox(&m_FilterInput, &FileBox, 10.0f))
		{
			RefreshFilteredFileList();
			if(m_vpFilteredFileList.empty())
			{
				m_SelectedFileIndex = -1;
			}
			else if(m_SelectedFileIndex == -1 ||
				(!m_FilterInput.IsEmpty() && !str_find_nocase(m_vpFilteredFileList[m_SelectedFileIndex]->m_aDisplayName, m_FilterInput.GetString())))
			{
				m_SelectedFileIndex = -1;
				for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
				{
					if(str_find_nocase(m_vpFilteredFileList[i]->m_aDisplayName, m_FilterInput.GetString()))
					{
						m_SelectedFileIndex = i;
						break;
					}
				}
				if(m_SelectedFileIndex == -1)
				{
					m_SelectedFileIndex = 0;
				}
			}
			str_copy(m_aSelectedFileDisplayName, m_SelectedFileIndex >= 0 ? m_vpFilteredFileList[m_SelectedFileIndex]->m_aDisplayName : "");
			UpdateFilenameInput();
			m_ListBox.ScrollToSelected();
			m_PreviewState = EPreviewState::UNLOADED;
		}
	}

	// File preview
	if(m_SelectedFileIndex >= 0 && CanPreviewFile())
	{
		UpdateFilePreview();
		Preview.Margin(10.0f, &Preview);
		RenderFilePreview(Preview);
	}

	// File list
	m_ListBox.DoStart(15.0f, m_vpFilteredFileList.size(), 1, 5, m_SelectedFileIndex, &View, false, IGraphics::CORNER_ALL, true);

	for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
	{
		const CListboxItem Item = m_ListBox.DoNextItem(m_vpFilteredFileList[i], m_SelectedFileIndex >= 0 && (size_t)m_SelectedFileIndex == i);
		if(!Item.m_Visible)
		{
			continue;
		}

		CUIRect Button, FileIcon, TimeModified;
		Item.m_Rect.VMargin(2.0f, &Button);
		Button.VSplitLeft(Button.h, &FileIcon, &Button);
		Button.VSplitLeft(5.0f, nullptr, &Button);
		Button.VSplitRight(100.0f, &Button, &TimeModified);
		Button.VSplitRight(5.0f, &Button, nullptr);

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
		Ui()->DoLabel(&FileIcon, DetermineFileFontIcon(m_vpFilteredFileList[i]), 12.0f, TEXTALIGN_ML);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		Ui()->DoLabel(&Button, m_vpFilteredFileList[i]->m_aDisplayName, 10.0f, TEXTALIGN_ML, {.m_MaxWidth = Button.w, .m_EllipsisAtEnd = true});

		if(!m_vpFilteredFileList[i]->m_IsLink && str_comp(m_vpFilteredFileList[i]->m_aFilename, "..") != 0)
		{
			char aLabelTimeModified[64];
			str_timestamp_ex(m_vpFilteredFileList[i]->m_TimeModified, aLabelTimeModified, sizeof(aLabelTimeModified), "%d.%m.%Y %H:%M");
			Ui()->DoLabel(&TimeModified, aLabelTimeModified, 10.0f, TEXTALIGN_MR);
		}
	}

	const int NewSelection = m_ListBox.DoEnd();
	if(NewSelection != m_SelectedFileIndex)
	{
		m_SelectedFileIndex = NewSelection;
		str_copy(m_aSelectedFileDisplayName, m_SelectedFileIndex >= 0 ? m_vpFilteredFileList[m_SelectedFileIndex]->m_aDisplayName : "");
		const bool WasChanged = m_FilenameInput.WasChanged();
		UpdateFilenameInput();
		if(!WasChanged) // ensure that changed flag is not set if it wasn't previously set, as this would reset the selection after DoEditBox is called
		{
			m_FilenameInput.WasChanged(); // this clears the changed flag
		}
		m_PreviewState = EPreviewState::UNLOADED;
	}

	// Buttons
	const float ButtonSpacing = ButtonBar.w > 600.0f ? 40.0f : 10.0f;

	CUIRect Button;
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	const bool IsDir = m_SelectedFileIndex >= 0 && m_vpFilteredFileList[m_SelectedFileIndex]->m_IsDir;
	const char *pOpenTooltip = IsDir ? "Open the selected folder." : (m_SaveAction ? "Save file with the specified name." : "Open the selected file.");
	if(Editor()->DoButton_Editor(&m_ButtonOkId, IsDir ? "Open" : m_aButtonText, 0, &Button, BUTTONFLAG_LEFT, pOpenTooltip) ||
		m_ListBox.WasItemActivated() ||
		(m_ListBox.Active() && Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		if(IsDir)
		{
			m_FilterInput.Clear();
			Ui()->SetActiveItem(&m_FilterInput);
			const bool ParentFolder = str_comp(m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename, "..") == 0;
			if(ParentFolder)
			{
				str_copy(m_aSelectedFileDisplayName, fs_filename(m_pCurrentPath));
				str_append(m_aSelectedFileDisplayName, "/");
				if(fs_parent_dir(m_pCurrentPath))
				{
					if(str_comp(m_pCurrentPath, m_aCurrentFolder) == 0)
					{
						m_ShowingRoot = true;
						if(m_StorageType == IStorage::TYPE_ALL)
						{
							m_aSelectedFileDisplayName[0] = '\0'; // will select first list item
						}
						else
						{
							Storage()->GetCompletePath(m_StorageType, m_pCurrentPath, m_aSelectedFileDisplayName, sizeof(m_aSelectedFileDisplayName));
							str_append(m_aSelectedFileDisplayName, "/");
						}
					}
					else
					{
						m_pCurrentPath = m_aCurrentFolder; // leave the link
						str_copy(m_aSelectedFileDisplayName, m_aCurrentLink);
						str_append(m_aSelectedFileDisplayName, "/");
					}
				}
			}
			else // sub folder
			{
				if(m_vpFilteredFileList[m_SelectedFileIndex]->m_IsLink)
				{
					m_pCurrentPath = m_aCurrentLink; // follow the link
					str_copy(m_aCurrentLink, m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename);
				}
				else
				{
					str_append(m_pCurrentPath, "/", IO_MAX_PATH_LENGTH);
					str_append(m_pCurrentPath, m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename, IO_MAX_PATH_LENGTH);
				}
				if(m_ShowingRoot)
				{
					m_StorageType = m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType;
				}
				m_ShowingRoot = false;
			}
			FilelistPopulate(m_StorageType, ParentFolder);
			UpdateFilenameInput();
		}
		else // file
		{
			const int StorageType = m_SelectedFileIndex >= 0 ? m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType : m_StorageType;
			char aSaveFilePath[IO_MAX_PATH_LENGTH];
			str_format(aSaveFilePath, sizeof(aSaveFilePath), "%s/%s", m_pCurrentPath, m_FilenameInput.GetString());
			if(!str_endswith(aSaveFilePath, FILETYPE_EXTENSIONS[(int)m_FileType]))
			{
				str_append(aSaveFilePath, FILETYPE_EXTENSIONS[(int)m_FileType]);
			}

			char aFilename[IO_MAX_PATH_LENGTH];
			fs_split_file_extension(fs_filename(aSaveFilePath), aFilename, sizeof(aFilename));
			if(m_SaveAction && !str_valid_filename(aFilename))
			{
				Editor()->ShowFileDialogError("This name cannot be used for files and folders.");
			}
			else if(m_SaveAction && Storage()->FileExists(aSaveFilePath, StorageType))
			{
				m_PopupConfirmOverwrite.m_pFileBrowser = this;
				str_copy(m_PopupConfirmOverwrite.m_aOverwritePath, aSaveFilePath);
				constexpr float PopupWidth = 400.0f;
				constexpr float PopupHeight = 150.0f;
				Ui()->DoPopupMenu(&m_PopupConfirmOverwrite,
					OriginalWidth / 2.0f - PopupWidth / 2.0f, OriginalHeight / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight,
					&m_PopupConfirmOverwrite, CPopupConfirmOverwrite::Render);
			}
			else if(m_pfnOpenCallback && (m_SaveAction || m_SelectedFileIndex >= 0))
			{
				m_pfnOpenCallback(aSaveFilePath, StorageType, m_pOpenCallbackUser);
			}
		}
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(Editor()->DoButton_Editor(&m_ButtonCancelId, "Cancel", 0, &Button, BUTTONFLAG_LEFT, "Close this dialog.") ||
		(m_ListBox.Active() && Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)))
	{
		Editor()->OnDialogClose();
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(Editor()->DoButton_Editor(&m_ButtonRefreshId, "Refresh", 0, &Button, BUTTONFLAG_LEFT, "Refresh the list of files.") ||
		(m_ListBox.Active() && (Input()->KeyIsPressed(KEY_F5) || (Input()->ModifierIsPressed() && Input()->KeyIsPressed(KEY_R)))))
	{
		FilelistPopulate(m_StorageType, true);
	}

	if(m_SelectedFileIndex >= 0 && m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType != IStorage::TYPE_ALL)
	{
		ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
		ButtonBar.VSplitRight(90.0f, &ButtonBar, &Button);
		if(Editor()->DoButton_Editor(&m_ButtonShowDirectoryId, "Show directory", 0, &Button, BUTTONFLAG_LEFT, "Open the current directory in the file browser."))
		{
			char aOpenPath[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType, m_pCurrentPath, aOpenPath, sizeof(aOpenPath));
			if(!Client()->ViewFile(aOpenPath))
			{
				Editor()->ShowFileDialogError("Failed to open the directory '%s'.", aOpenPath);
			}
		}
	}

	ButtonBar.VSplitRight(ButtonSpacing, &ButtonBar, nullptr);
	ButtonBar.VSplitRight(50.0f, &ButtonBar, &Button);
	if(m_SelectedFileIndex >= 0 &&
		m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType == IStorage::TYPE_SAVE &&
		!m_vpFilteredFileList[m_SelectedFileIndex]->m_IsLink &&
		str_comp(m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename, "..") != 0)
	{
		if(Editor()->DoButton_Editor(&m_ButtonDeleteId, "Delete", 0, &Button, BUTTONFLAG_LEFT, IsDir ? "Delete the selected folder." : "Delete the selected file.") ||
			(m_ListBox.Active() && Ui()->ConsumeHotkey(CUi::HOTKEY_DELETE)))
		{
			m_PopupConfirmDelete.m_pFileBrowser = this;
			m_PopupConfirmDelete.m_IsDirectory = IsDir;
			str_format(m_PopupConfirmDelete.m_aDeletePath, sizeof(m_PopupConfirmDelete.m_aDeletePath), "%s/%s", m_pCurrentPath, m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename);
			constexpr float PopupWidth = 400.0f;
			constexpr float PopupHeight = 150.0f;
			Ui()->DoPopupMenu(&m_PopupConfirmDelete,
				OriginalWidth / 2.0f - PopupWidth / 2.0f, OriginalHeight / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight,
				&m_PopupConfirmDelete, CPopupConfirmDelete::Render);
		}
	}

	if(!m_ShowingRoot && m_StorageType == IStorage::TYPE_SAVE)
	{
		ButtonBar.VSplitLeft(70.0f, &Button, &ButtonBar);
		if(Editor()->DoButton_Editor(&m_ButtonNewFolderId, "New folder", 0, &Button, BUTTONFLAG_LEFT, "Create a new folder."))
		{
			m_PopupNewFolder.m_pFileBrowser = this;
			m_PopupNewFolder.m_NewFolderNameInput.Clear();
			constexpr float PopupWidth = 400.0f;
			constexpr float PopupHeight = 110.0f;
			Ui()->DoPopupMenu(&m_PopupNewFolder,
				OriginalWidth / 2.0f - PopupWidth / 2.0f, OriginalHeight / 2.0f - PopupHeight / 2.0f, PopupWidth, PopupHeight,
				&m_PopupNewFolder, CPopupNewFolder::Render);
			Ui()->SetActiveItem(&m_PopupNewFolder.m_NewFolderNameInput);
		}
	}
}

bool CFileBrowser::IsValidSaveFilename() const
{
	return m_pCurrentPath == m_aCurrentFolder ||
	       (m_pCurrentPath == m_aCurrentLink && str_startswith(m_aCurrentLink, "themes"));
}

void CFileBrowser::OnEditorClose()
{
	if(m_PreviewSound >= 0 && Sound()->IsPlaying(m_PreviewSound))
	{
		Sound()->Pause(m_PreviewSound);
	}
}

void CFileBrowser::OnDialogClose()
{
	m_PreviewState = EPreviewState::UNLOADED;
	Graphics()->UnloadTexture(&m_PreviewImage);
	m_PreviewImageWidth = -1;
	m_PreviewImageHeight = -1;
	Sound()->UnloadSample(m_PreviewSound);
	m_PreviewSound = -1;
}

bool CFileBrowser::CanPreviewFile() const
{
	return m_FileType == CFileBrowser::EFileType::IMAGE ||
	       m_FileType == CFileBrowser::EFileType::SOUND;
}

void CFileBrowser::UpdateFilePreview()
{
	if(m_PreviewState != EPreviewState::UNLOADED ||
		!str_endswith(m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename, FILETYPE_EXTENSIONS[(int)m_FileType]))
	{
		return;
	}

	if(m_FileType == CFileBrowser::EFileType::IMAGE)
	{
		char aImagePath[IO_MAX_PATH_LENGTH];
		str_format(aImagePath, sizeof(aImagePath), "%s/%s", m_pCurrentPath, m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename);
		CImageInfo PreviewImageInfo;
		if(Graphics()->LoadPng(PreviewImageInfo, aImagePath, m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType))
		{
			Graphics()->UnloadTexture(&m_PreviewImage);
			m_PreviewImageWidth = PreviewImageInfo.m_Width;
			m_PreviewImageHeight = PreviewImageInfo.m_Height;
			m_PreviewImage = Graphics()->LoadTextureRawMove(PreviewImageInfo, 0, aImagePath);
			m_PreviewState = EPreviewState::LOADED;
		}
		else
		{
			m_PreviewState = EPreviewState::ERROR;
		}
	}
	else if(m_FileType == CFileBrowser::EFileType::SOUND)
	{
		char aSoundPath[IO_MAX_PATH_LENGTH];
		str_format(aSoundPath, sizeof(aSoundPath), "%s/%s", m_pCurrentPath, m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename);
		Sound()->UnloadSample(m_PreviewSound);
		m_PreviewSound = Sound()->LoadOpus(aSoundPath, m_vpFilteredFileList[m_SelectedFileIndex]->m_StorageType);
		m_PreviewState = m_PreviewSound == -1 ? EPreviewState::ERROR : EPreviewState::LOADED;
	}
}

void CFileBrowser::RenderFilePreview(CUIRect Preview)
{
	if(m_FileType == CFileBrowser::EFileType::IMAGE)
	{
		if(m_PreviewState == EPreviewState::LOADED)
		{
			CUIRect PreviewLabel, PreviewImage;
			Preview.HSplitTop(20.0f, &PreviewLabel, &PreviewImage);

			char aSizeLabel[64];
			str_format(aSizeLabel, sizeof(aSizeLabel), "Size: %d × %d", m_PreviewImageWidth, m_PreviewImageHeight);
			Ui()->DoLabel(&PreviewLabel, aSizeLabel, 12.0f, TEXTALIGN_ML);

			int Width = m_PreviewImageWidth;
			int Height = m_PreviewImageHeight;
			if(m_PreviewImageWidth > PreviewImage.w)
			{
				Height = m_PreviewImageHeight * PreviewImage.w / m_PreviewImageWidth;
				Width = PreviewImage.w;
			}
			if(Height > PreviewImage.h)
			{
				Width = Width * PreviewImage.h / Height;
				Height = PreviewImage.h;
			}

			Graphics()->TextureSet(m_PreviewImage);
			Graphics()->BlendNormal();
			Graphics()->QuadsBegin();
			IGraphics::CQuadItem QuadItem(PreviewImage.x, PreviewImage.y, Width, Height);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
		else if(m_PreviewState == EPreviewState::ERROR)
		{
			Ui()->DoLabel(&Preview, "Failed to load the image (check the local console for details).", 12.0f, TEXTALIGN_TL, {.m_MaxWidth = Preview.w});
		}
	}
	else if(m_FileType == CFileBrowser::EFileType::SOUND)
	{
		if(m_PreviewState == EPreviewState::LOADED)
		{
			Preview.HSplitTop(20.0f, &Preview, nullptr);
			Preview.VSplitLeft(Preview.h / 4.0f, nullptr, &Preview);
			Editor()->DoAudioPreview(Preview, &m_ButtonPlayPauseId, &m_ButtonStopId, &m_SeekBarId, m_PreviewSound);
		}
		else if(m_PreviewState == EPreviewState::ERROR)
		{
			Ui()->DoLabel(&Preview, "Failed to load the sound (check the local console for details). Make sure you enabled sounds in the settings.", 12.0f, TEXTALIGN_TL, {.m_MaxWidth = Preview.w});
		}
	}
}

const char *CFileBrowser::DetermineFileFontIcon(const CFilelistItem *pItem) const
{
	if(!pItem->m_IsDir)
	{
		switch(m_FileType)
		{
		case EFileType::MAP:
			return FONT_ICON_MAP;
		case EFileType::IMAGE:
			return FONT_ICON_IMAGE;
		case EFileType::SOUND:
			return FONT_ICON_MUSIC;
		default:
			dbg_assert(false, "m_FileType invalid: %d", (int)m_FileType);
			dbg_break();
		}
	}
	else if(pItem->m_IsLink || str_comp(pItem->m_aFilename, "..") == 0)
	{
		return FONT_ICON_FOLDER_TREE;
	}
	else
	{
		return FONT_ICON_FOLDER;
	}
}

void CFileBrowser::UpdateFilenameInput()
{
	if(m_SelectedFileIndex >= 0 && !m_vpFilteredFileList[m_SelectedFileIndex]->m_IsDir)
	{
		char aNameWithoutExt[IO_MAX_PATH_LENGTH];
		fs_split_file_extension(m_vpFilteredFileList[m_SelectedFileIndex]->m_aFilename, aNameWithoutExt, sizeof(aNameWithoutExt));
		m_FilenameInput.Set(aNameWithoutExt);
	}
	else
	{
		m_FilenameInput.Clear();
	}
}

void CFileBrowser::UpdateSelectedIndex(const char *pDisplayName)
{
	m_SelectedFileIndex = -1;
	m_aSelectedFileDisplayName[0] = '\0';
	for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
	{
		if(str_comp_nocase(m_vpFilteredFileList[i]->m_aDisplayName, pDisplayName) == 0)
		{
			m_SelectedFileIndex = i;
			str_copy(m_aSelectedFileDisplayName, m_vpFilteredFileList[i]->m_aDisplayName);
			break;
		}
	}
	if(m_SelectedFileIndex >= 0)
	{
		m_ListBox.ScrollToSelected();
	}
}

void CFileBrowser::SortFilteredFileList()
{
	if(m_SortByFilename == ESortDirection::ASCENDING)
	{
		std::sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CFileBrowser::CompareFilenameAscending);
	}
	else
	{
		std::sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CFileBrowser::CompareFilenameDescending);
	}

	if(m_SortByTimeModified == ESortDirection::ASCENDING)
	{
		std::stable_sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CFileBrowser::CompareTimeModifiedAscending);
	}
	else if(m_SortByTimeModified == ESortDirection::DESCENDING)
	{
		std::stable_sort(m_vpFilteredFileList.begin(), m_vpFilteredFileList.end(), CFileBrowser::CompareTimeModifiedDescending);
	}
}

void CFileBrowser::RefreshFilteredFileList()
{
	m_vpFilteredFileList.clear();
	for(const CFilelistItem &Item : m_vCompleteFileList)
	{
		if(m_FilterInput.IsEmpty() || str_find_nocase(Item.m_aDisplayName, m_FilterInput.GetString()))
		{
			m_vpFilteredFileList.push_back(&Item);
		}
	}
	if(!m_ShowingRoot)
	{
		SortFilteredFileList();
	}
	if(!m_vpFilteredFileList.empty())
	{
		if(m_aSelectedFileDisplayName[0] != '\0')
		{
			for(size_t i = 0; i < m_vpFilteredFileList.size(); i++)
			{
				if(str_comp(m_vpFilteredFileList[i]->m_aDisplayName, m_aSelectedFileDisplayName) == 0)
				{
					m_SelectedFileIndex = i;
					break;
				}
			}
		}
		m_SelectedFileIndex = std::clamp<int>(m_SelectedFileIndex, 0, m_vpFilteredFileList.size() - 1);
		str_copy(m_aSelectedFileDisplayName, m_vpFilteredFileList[m_SelectedFileIndex]->m_aDisplayName);
	}
	else
	{
		m_SelectedFileIndex = -1;
		m_aSelectedFileDisplayName[0] = '\0';
	}
}

void CFileBrowser::FilelistPopulate(int StorageType, bool KeepSelection)
{
	m_vCompleteFileList.clear();
	if(m_ShowingRoot)
	{
		{
			CFilelistItem Item;
			str_copy(Item.m_aFilename, m_pCurrentPath);
			str_copy(Item.m_aDisplayName, "All combined");
			Item.m_IsDir = true;
			Item.m_IsLink = true;
			Item.m_StorageType = IStorage::TYPE_ALL;
			Item.m_TimeModified = 0;
			m_vCompleteFileList.push_back(Item);
		}

		for(int CheckStorageType = IStorage::TYPE_SAVE; CheckStorageType < Storage()->NumPaths(); ++CheckStorageType)
		{
			if(Storage()->FolderExists(m_pCurrentPath, CheckStorageType))
			{
				CFilelistItem Item;
				str_copy(Item.m_aFilename, m_pCurrentPath);
				Storage()->GetCompletePath(CheckStorageType, m_pCurrentPath, Item.m_aDisplayName, sizeof(Item.m_aDisplayName));
				str_append(Item.m_aDisplayName, "/", sizeof(Item.m_aDisplayName));
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = CheckStorageType;
				Item.m_TimeModified = 0;
				m_vCompleteFileList.push_back(Item);
			}
		}
	}
	else
	{
		// Add links for downloadedmaps and themes
		if(!str_comp(m_pCurrentPath, "maps"))
		{
			if(!m_SaveAction && Storage()->FolderExists("downloadedmaps", StorageType))
			{
				CFilelistItem Item;
				str_copy(Item.m_aFilename, "downloadedmaps");
				str_copy(Item.m_aDisplayName, "downloadedmaps/");
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = StorageType;
				Item.m_TimeModified = 0;
				m_vCompleteFileList.push_back(Item);
			}

			if(Storage()->FolderExists("themes", StorageType))
			{
				CFilelistItem Item;
				str_copy(Item.m_aFilename, "themes");
				str_copy(Item.m_aDisplayName, "themes/");
				Item.m_IsDir = true;
				Item.m_IsLink = true;
				Item.m_StorageType = StorageType;
				Item.m_TimeModified = 0;
				m_vCompleteFileList.push_back(Item);
			}
		}
		Storage()->ListDirectoryInfo(StorageType, m_pCurrentPath, DirectoryListingCallback, this);
	}
	RefreshFilteredFileList();
	if(!KeepSelection)
	{
		m_SelectedFileIndex = m_vpFilteredFileList.empty() ? -1 : 0;
		str_copy(m_aSelectedFileDisplayName, m_SelectedFileIndex >= 0 ? m_vpFilteredFileList[m_SelectedFileIndex]->m_aDisplayName : "");
	}
	m_PreviewState = EPreviewState::UNLOADED;
}

int CFileBrowser::DirectoryListingCallback(const CFsFileInfo *pInfo, int IsDir, int StorageType, void *pUser)
{
	CFileBrowser *pFileBrowser = static_cast<CFileBrowser *>(pUser);
	if(IsDir)
	{
		if(str_comp(pInfo->m_pName, ".") == 0)
		{
			return 0;
		}
		if(str_comp(pInfo->m_pName, "..") == 0 &&
			!pFileBrowser->m_MultipleStorages &&
			str_comp(pFileBrowser->m_aInitialFolder, pFileBrowser->m_pCurrentPath) == 0)
		{
			return 0;
		}
	}
	else
	{
		if(!str_endswith(pInfo->m_pName, FILETYPE_EXTENSIONS[(int)pFileBrowser->m_FileType]))
		{
			return 0;
		}
	}

	CFilelistItem Item;
	str_copy(Item.m_aFilename, pInfo->m_pName);
	if(IsDir)
	{
		str_format(Item.m_aDisplayName, sizeof(Item.m_aDisplayName), "%s/", pInfo->m_pName);
	}
	else
	{
		fs_split_file_extension(pInfo->m_pName, Item.m_aDisplayName, sizeof(Item.m_aDisplayName));
	}
	Item.m_IsDir = IsDir != 0;
	Item.m_IsLink = false;
	Item.m_StorageType = StorageType;
	Item.m_TimeModified = pInfo->m_TimeModified;
	pFileBrowser->m_vCompleteFileList.push_back(Item);
	return 0;
}

std::optional<bool> CFileBrowser::CompareCommon(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
{
	if(str_comp(pLhs->m_aFilename, "..") == 0)
	{
		return true;
	}
	if(str_comp(pRhs->m_aFilename, "..") == 0)
	{
		return false;
	}
	if(pLhs->m_IsLink != pRhs->m_IsLink)
	{
		return pLhs->m_IsLink;
	}
	if(pLhs->m_IsDir != pRhs->m_IsDir)
	{
		return pLhs->m_IsDir;
	}
	return std::nullopt;
}

bool CFileBrowser::CompareFilenameAscending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
{
	return CompareCommon(pLhs, pRhs).value_or(str_comp_filenames(pLhs->m_aDisplayName, pRhs->m_aDisplayName) < 0);
}

bool CFileBrowser::CompareFilenameDescending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
{
	return CompareCommon(pLhs, pRhs).value_or(str_comp_filenames(pLhs->m_aDisplayName, pRhs->m_aDisplayName) > 0);
}

bool CFileBrowser::CompareTimeModifiedAscending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
{
	return CompareCommon(pLhs, pRhs).value_or(pLhs->m_TimeModified < pRhs->m_TimeModified);
}

bool CFileBrowser::CompareTimeModifiedDescending(const CFilelistItem *pLhs, const CFilelistItem *pRhs)
{
	return CompareCommon(pLhs, pRhs).value_or(pLhs->m_TimeModified > pRhs->m_TimeModified);
}

CUi::EPopupMenuFunctionResult CFileBrowser::CPopupNewFolder::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupNewFolder *pNewFolderContext = static_cast<CPopupNewFolder *>(pContext);
	CFileBrowser *pFileBrowser = pNewFolderContext->m_pFileBrowser;
	CEditor *pEditor = pFileBrowser->Editor();

	CUIRect Label, ButtonBar, FolderName, ButtonCancel, ButtonCreate;

	View.Margin(10.0f, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);
	ButtonBar.VSplitLeft(110.0f, &ButtonCancel, &ButtonBar);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &ButtonCreate);

	View.HSplitTop(20.0f, &Label, &View);
	pFileBrowser->Ui()->DoLabel(&Label, "Create new folder", 20.0f, TEXTALIGN_MC);
	View.HSplitTop(10.0f, nullptr, &View);

	View.HSplitTop(20.0f, &Label, &View);
	pFileBrowser->Ui()->DoLabel(&Label, "Name:", 10.0f, TEXTALIGN_ML);
	Label.VSplitLeft(50.0f, nullptr, &FolderName);
	FolderName.HMargin(2.0f, &FolderName);
	pEditor->DoEditBox(&pNewFolderContext->m_NewFolderNameInput, &FolderName, 12.0f);

	if(pEditor->DoButton_Editor(&pNewFolderContext->m_ButtonCancelId, "Cancel", 0, &ButtonCancel, BUTTONFLAG_LEFT, nullptr))
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}

	if(pEditor->DoButton_Editor(&pNewFolderContext->m_ButtonCreateId, "Create", 0, &ButtonCreate, BUTTONFLAG_LEFT, nullptr) ||
		(Active && pFileBrowser->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		char aFolderPath[IO_MAX_PATH_LENGTH];
		str_format(aFolderPath, sizeof(aFolderPath), "%s/%s", pFileBrowser->m_pCurrentPath, pNewFolderContext->m_NewFolderNameInput.GetString());
		if(!str_valid_filename(pNewFolderContext->m_NewFolderNameInput.GetString()))
		{
			pEditor->ShowFileDialogError("This name cannot be used for files and folders.");
		}
		else if(!pFileBrowser->Storage()->CreateFolder(aFolderPath, pFileBrowser->m_StorageType))
		{
			pEditor->ShowFileDialogError("Failed to create the folder '%s'.", aFolderPath);
		}
		else
		{
			char aFolderDisplayName[IO_MAX_PATH_LENGTH];
			str_format(aFolderDisplayName, sizeof(aFolderDisplayName), "%s/", pNewFolderContext->m_NewFolderNameInput.GetString());
			pFileBrowser->FilelistPopulate(pFileBrowser->m_StorageType, false);
			pFileBrowser->UpdateSelectedIndex(aFolderDisplayName);
			return CUi::POPUP_CLOSE_CURRENT;
		}
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CFileBrowser::CPopupConfirmDelete::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupConfirmDelete *pConfirmDeleteContext = static_cast<CPopupConfirmDelete *>(pContext);
	CFileBrowser *pFileBrowser = pConfirmDeleteContext->m_pFileBrowser;
	CEditor *pEditor = pFileBrowser->Editor();

	CUIRect Label, ButtonBar, ButtonCancel, ButtonDelete;
	View.Margin(10.0f, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);
	View.HSplitTop(20.0f, &Label, &View);
	ButtonBar.VSplitLeft(110.0f, &ButtonCancel, &ButtonBar);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &ButtonDelete);

	pFileBrowser->Ui()->DoLabel(&Label, "Confirm delete", 20.0f, TEXTALIGN_MC);

	char aMessage[IO_MAX_PATH_LENGTH + 128];
	str_format(aMessage, sizeof(aMessage), "Are you sure that you want to delete the %s '%s'?",
		pConfirmDeleteContext->m_IsDirectory ? "folder" : "file", pConfirmDeleteContext->m_aDeletePath);
	pFileBrowser->Ui()->DoLabel(&View, aMessage, 10.0f, TEXTALIGN_ML, {.m_MaxWidth = View.w});

	if(pEditor->DoButton_Editor(&pConfirmDeleteContext->m_ButtonCancelId, "Cancel", 0, &ButtonCancel, BUTTONFLAG_LEFT, nullptr))
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}

	if(pEditor->DoButton_Editor(&pConfirmDeleteContext->m_ButtonDeleteId, "Delete", EditorButtonChecked::DANGEROUS_ACTION, &ButtonDelete, BUTTONFLAG_LEFT, nullptr) ||
		(Active && pFileBrowser->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		if(pConfirmDeleteContext->m_IsDirectory)
		{
			if(pFileBrowser->Storage()->RemoveFolder(pConfirmDeleteContext->m_aDeletePath, IStorage::TYPE_SAVE))
			{
				pFileBrowser->FilelistPopulate(IStorage::TYPE_SAVE, true);
			}
			else
			{
				pEditor->ShowFileDialogError("Failed to delete folder '%s'. Make sure it's empty first. Check the local console for details.", pConfirmDeleteContext->m_aDeletePath);
			}
		}
		else
		{
			if(pFileBrowser->Storage()->RemoveFile(pConfirmDeleteContext->m_aDeletePath, IStorage::TYPE_SAVE))
			{
				pFileBrowser->FilelistPopulate(IStorage::TYPE_SAVE, true);
			}
			else
			{
				pEditor->ShowFileDialogError("Failed to delete file '%s'. Check the local console for details.", pConfirmDeleteContext->m_aDeletePath);
			}
		}
		pFileBrowser->UpdateFilenameInput();
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CFileBrowser::CPopupConfirmOverwrite::Render(void *pContext, CUIRect View, bool Active)
{
	CPopupConfirmOverwrite *pConfirmOverwriteContext = static_cast<CPopupConfirmOverwrite *>(pContext);
	CFileBrowser *pFileBrowser = pConfirmOverwriteContext->m_pFileBrowser;
	CEditor *pEditor = pFileBrowser->Editor();

	CUIRect Label, ButtonBar, ButtonCancel, ButtonOverride;
	View.Margin(10.0f, &View);
	View.HSplitBottom(20.0f, &View, &ButtonBar);
	View.HSplitTop(20.0f, &Label, &View);
	ButtonBar.VSplitLeft(110.0f, &ButtonCancel, &ButtonBar);
	ButtonBar.VSplitRight(110.0f, &ButtonBar, &ButtonOverride);

	pFileBrowser->Ui()->DoLabel(&Label, "Confirm overwrite", 20.0f, TEXTALIGN_MC);

	char aMessage[IO_MAX_PATH_LENGTH + 128];
	str_format(aMessage, sizeof(aMessage), "The file '%s' already exists.\n\nAre you sure that you want to overwrite it?",
		pConfirmOverwriteContext->m_aOverwritePath);
	pFileBrowser->Ui()->DoLabel(&View, aMessage, 10.0f, TEXTALIGN_ML, {.m_MaxWidth = View.w});

	if(pEditor->DoButton_Editor(&pConfirmOverwriteContext->m_ButtonCancelId, "Cancel", 0, &ButtonCancel, BUTTONFLAG_LEFT, nullptr))
	{
		return CUi::POPUP_CLOSE_CURRENT;
	}

	if(pEditor->DoButton_Editor(&pConfirmOverwriteContext->m_ButtonOverwriteId, "Overwrite", EditorButtonChecked::DANGEROUS_ACTION, &ButtonOverride, BUTTONFLAG_LEFT, nullptr) ||
		(Active && pFileBrowser->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)))
	{
		pFileBrowser->m_pfnOpenCallback(pConfirmOverwriteContext->m_aOverwritePath, IStorage::TYPE_SAVE, pFileBrowser->m_pOpenCallbackUser);
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}
