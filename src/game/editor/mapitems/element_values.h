#ifndef GAME_EDITOR_MAPITEMS_ELEMENT_VALUES_H
#define GAME_EDITOR_MAPITEMS_ELEMENT_VALUES_H

#include <game/mapitems.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>

/** Canonical document reference. Zero is explicit absence, never an index. */
class CDocumentReference
{
public:
	std::uint64_t m_Id = 0;
	bool operator==(const CDocumentReference &) const = default;
	bool IsSet() const { return m_Id != 0; }
};

/** Editor identity is never embedded in game/map ABI structures. */
class CDocumentIdentity
{
public:
	std::uint64_t m_Id = 0;
	bool operator==(const CDocumentIdentity &) const = default;
};

/** Owned by a document lifetime, outside its snapshot and equality. */
class CDocumentIdentityAllocator
{
	std::uint64_t m_Next = 1;

public:
	std::optional<std::uint64_t> Allocate()
	{
		if(m_Next == std::numeric_limits<std::uint64_t>::max())
			return std::nullopt;
		return m_Next++;
	}

	bool ReserveThrough(std::uint64_t Id)
	{
		if(Id == std::numeric_limits<std::uint64_t>::max())
			return false;
		m_Next = std::max(m_Next, Id + 1);
		return true;
	}
};

/** Equality for ABI data is semantic; it never examines padding bytes. */
inline bool EditorFormatEqual(const CQuad &Left, const CQuad &Right)
{
	return std::equal(std::begin(Left.m_aPoints), std::end(Left.m_aPoints), std::begin(Right.m_aPoints)) &&
	       std::equal(std::begin(Left.m_aColors), std::end(Left.m_aColors), std::begin(Right.m_aColors)) &&
	       std::equal(std::begin(Left.m_aTexcoords), std::end(Left.m_aTexcoords), std::begin(Right.m_aTexcoords)) &&
	       Left.m_PosEnv == Right.m_PosEnv && Left.m_PosEnvOffset == Right.m_PosEnvOffset &&
	       Left.m_ColorEnv == Right.m_ColorEnv && Left.m_ColorEnvOffset == Right.m_ColorEnvOffset;
}

inline bool EditorFormatEqual(const CSoundSource &Left, const CSoundSource &Right)
{
	if(Left.m_Position != Right.m_Position || Left.m_Loop != Right.m_Loop || Left.m_Pan != Right.m_Pan ||
		Left.m_TimeDelay != Right.m_TimeDelay || Left.m_Falloff != Right.m_Falloff ||
		Left.m_PosEnv != Right.m_PosEnv || Left.m_PosEnvOffset != Right.m_PosEnvOffset ||
		Left.m_SoundEnv != Right.m_SoundEnv || Left.m_SoundEnvOffset != Right.m_SoundEnvOffset ||
		Left.m_Shape.m_Type != Right.m_Shape.m_Type)
		return false;
	if(Left.m_Shape.m_Type == CSoundShape::SHAPE_CIRCLE)
		return Left.m_Shape.m_Circle.m_Radius == Right.m_Shape.m_Circle.m_Radius;
	return Left.m_Shape.m_Rectangle.m_Width == Right.m_Shape.m_Rectangle.m_Width &&
	       Left.m_Shape.m_Rectangle.m_Height == Right.m_Shape.m_Rectangle.m_Height;
}

inline bool EditorFormatEqual(const CEnvPoint_runtime &Left, const CEnvPoint_runtime &Right)
{
	return Left.m_Time == Right.m_Time && Left.m_Curvetype == Right.m_Curvetype &&
	       std::equal(std::begin(Left.m_aValues), std::end(Left.m_aValues), std::begin(Right.m_aValues)) &&
	       std::equal(std::begin(Left.m_Bezier.m_aInTangentDeltaX), std::end(Left.m_Bezier.m_aInTangentDeltaX), std::begin(Right.m_Bezier.m_aInTangentDeltaX)) &&
	       std::equal(std::begin(Left.m_Bezier.m_aInTangentDeltaY), std::end(Left.m_Bezier.m_aInTangentDeltaY), std::begin(Right.m_Bezier.m_aInTangentDeltaY)) &&
	       std::equal(std::begin(Left.m_Bezier.m_aOutTangentDeltaX), std::end(Left.m_Bezier.m_aOutTangentDeltaX), std::begin(Right.m_Bezier.m_aOutTangentDeltaX)) &&
	       std::equal(std::begin(Left.m_Bezier.m_aOutTangentDeltaY), std::end(Left.m_Bezier.m_aOutTangentDeltaY), std::begin(Right.m_Bezier.m_aOutTangentDeltaY));
}

template<typename T>
class CEditorFormatValues : public T
{
public:
	CEditorFormatValues() :
		T{} {}
	explicit CEditorFormatValues(const T &Value) :
		T(Value) {}
	bool operator==(const CEditorFormatValues &Other) const { return EditorFormatEqual(*this, Other); }
};

class CQuadValues : public CDocumentIdentity
{
public:
	CPoint m_aPoints[5]{};
	CColor m_aColors[4]{};
	CPoint m_aTexcoords[4]{};
	CDocumentReference m_PosEnv;
	int m_PosEnvOffset = 0;
	CDocumentReference m_ColorEnv;
	int m_ColorEnvOffset = 0;
	bool operator==(const CQuadValues &) const = default;

	template<typename F>
	static CQuadValues Import(const CQuad &Quad, F &&EnvelopeReference)
	{
		CQuadValues Result;
		std::copy(std::begin(Quad.m_aPoints), std::end(Quad.m_aPoints), Result.m_aPoints);
		std::copy(std::begin(Quad.m_aColors), std::end(Quad.m_aColors), Result.m_aColors);
		std::copy(std::begin(Quad.m_aTexcoords), std::end(Quad.m_aTexcoords), Result.m_aTexcoords);
		Result.m_PosEnv = EnvelopeReference(Quad.m_PosEnv);
		Result.m_PosEnvOffset = Quad.m_PosEnvOffset;
		Result.m_ColorEnv = EnvelopeReference(Quad.m_ColorEnv);
		Result.m_ColorEnvOffset = Quad.m_ColorEnvOffset;
		return Result;
	}

	template<typename F>
	CQuad Export(F &&EnvelopeIndex) const
	{
		CQuad Result{};
		std::copy(std::begin(m_aPoints), std::end(m_aPoints), Result.m_aPoints);
		std::copy(std::begin(m_aColors), std::end(m_aColors), Result.m_aColors);
		std::copy(std::begin(m_aTexcoords), std::end(m_aTexcoords), Result.m_aTexcoords);
		Result.m_PosEnv = EnvelopeIndex(m_PosEnv);
		Result.m_PosEnvOffset = m_PosEnvOffset;
		Result.m_ColorEnv = EnvelopeIndex(m_ColorEnv);
		Result.m_ColorEnvOffset = m_ColorEnvOffset;
		return Result;
	}
};

/** Active shape equality excludes inactive ABI union storage. */
inline bool EditorFormatEqual(const CSoundShape &Left, const CSoundShape &Right)
{
	if(Left.m_Type != Right.m_Type)
		return false;
	if(Left.m_Type == CSoundShape::SHAPE_CIRCLE)
		return Left.m_Circle.m_Radius == Right.m_Circle.m_Radius;
	return Left.m_Rectangle.m_Width == Right.m_Rectangle.m_Width && Left.m_Rectangle.m_Height == Right.m_Rectangle.m_Height;
}

class CSoundShapeValues : public CEditorFormatValues<CSoundShape>
{
public:
	bool operator==(const CSoundShapeValues &) const = default;
};

class CSoundSourceValues : public CDocumentIdentity
{
public:
	CPoint m_Position{};
	int m_Loop = 0;
	int m_Pan = 0;
	int m_TimeDelay = 0;
	int m_Falloff = 0;
	CDocumentReference m_PosEnv;
	int m_PosEnvOffset = 0;
	CDocumentReference m_SoundEnv;
	int m_SoundEnvOffset = 0;
	CSoundShapeValues m_Shape;
	bool operator==(const CSoundSourceValues &) const = default;

	template<typename F>
	static CSoundSourceValues Import(const CSoundSource &Source, F &&EnvelopeReference)
	{
		CSoundSourceValues Result;
		Result.m_Position = Source.m_Position;
		Result.m_Loop = Source.m_Loop;
		Result.m_Pan = Source.m_Pan;
		Result.m_TimeDelay = Source.m_TimeDelay;
		Result.m_Falloff = Source.m_Falloff;
		Result.m_PosEnv = EnvelopeReference(Source.m_PosEnv);
		Result.m_PosEnvOffset = Source.m_PosEnvOffset;
		Result.m_SoundEnv = EnvelopeReference(Source.m_SoundEnv);
		Result.m_SoundEnvOffset = Source.m_SoundEnvOffset;
		Result.m_Shape.m_Type = Source.m_Shape.m_Type;
		if(Source.m_Shape.m_Type == CSoundShape::SHAPE_CIRCLE)
			Result.m_Shape.m_Circle.m_Radius = Source.m_Shape.m_Circle.m_Radius;
		else
		{
			Result.m_Shape.m_Rectangle.m_Width = Source.m_Shape.m_Rectangle.m_Width;
			Result.m_Shape.m_Rectangle.m_Height = Source.m_Shape.m_Rectangle.m_Height;
		}
		return Result;
	}

	template<typename F>
	CSoundSource Export(F &&EnvelopeIndex) const
	{
		CSoundSource Result{};
		Result.m_Position = m_Position;
		Result.m_Loop = m_Loop;
		Result.m_Pan = m_Pan;
		Result.m_TimeDelay = m_TimeDelay;
		Result.m_Falloff = m_Falloff;
		Result.m_PosEnv = EnvelopeIndex(m_PosEnv);
		Result.m_PosEnvOffset = m_PosEnvOffset;
		Result.m_SoundEnv = EnvelopeIndex(m_SoundEnv);
		Result.m_SoundEnvOffset = m_SoundEnvOffset;
		Result.m_Shape.m_Type = m_Shape.m_Type;
		if(m_Shape.m_Type == CSoundShape::SHAPE_CIRCLE)
			Result.m_Shape.m_Circle.m_Radius = m_Shape.m_Circle.m_Radius;
		else
		{
			Result.m_Shape.m_Rectangle.m_Width = m_Shape.m_Rectangle.m_Width;
			Result.m_Shape.m_Rectangle.m_Height = m_Shape.m_Rectangle.m_Height;
		}
		return Result;
	}
};

class CEnvelopePointValues : public CEditorFormatValues<CEnvPoint_runtime>, public CDocumentIdentity
{
public:
	bool operator==(const CEnvelopePointValues &) const = default;
};

#endif
