#ifndef GAME_EDITOR_EDITOR_BUTTON_H
#define GAME_EDITOR_EDITOR_BUTTON_H

	typedef void (*FButtonCallback)(void *pEditor);
	class CEditorButton
	{
		public:
		const char *m_pText;
		FButtonCallback m_pfnCallback;
		void *m_pEditor;

		CEditorButton(const char *pText, FButtonCallback pfnCallback, void *pEditor) : m_pText(pText), m_pfnCallback(pfnCallback), m_pEditor(pEditor)
		{
		}

		void Call() const
		{
			m_pfnCallback(m_pEditor);
		}
	};

#endif
