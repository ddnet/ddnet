#ifndef GAME_EDITOR_MAP_OBJECT_H
#define GAME_EDITOR_MAP_OBJECT_H

class CEditor;
class CEditorMap;
class IGraphics;
class ISound;
class IStorage;
class ITextRender;

class CMapObject
{
public:
	explicit CMapObject(CEditorMap *pMap);
	CMapObject(const CMapObject &Other);
	virtual ~CMapObject() = default;

	virtual void OnAttach(CEditorMap *pMap);

	const CEditor *Editor() const;
	CEditor *Editor();
	const CEditorMap *Map() const;
	CEditorMap *Map();
	const IGraphics *Graphics() const;
	IGraphics *Graphics();
	const ISound *Sound() const;
	ISound *Sound();
	const IStorage *Storage() const;
	IStorage *Storage();
	const ITextRender *TextRender() const;
	ITextRender *TextRender();

private:
	CEditorMap *m_pMap;
};

#endif
