/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_JSONWRITER_H
#define ENGINE_SHARED_JSONWRITER_H

#include <base/types.h>

#include <stack>
#include <string>

/**
 * JSON writer with abstract writing function.
 */
class CJsonWriter
{
	enum EJsonStateKind
	{
		STATE_OBJECT,
		STATE_ARRAY,
		STATE_ATTRIBUTE,
	};

	struct SState
	{
		EJsonStateKind m_Kind;
		bool m_Empty = true;

		SState(EJsonStateKind Kind) :
			m_Kind(Kind)
		{
		}
	};

	std::stack<SState> m_States;
	int m_Indentation;

	bool CanWriteDatatype();
	void WriteInternalEscaped(const char *pStr);
	void WriteIndent(bool EndElement);
	void PushState(EJsonStateKind NewState);
	SState *TopState();
	EJsonStateKind PopState();
	void CompleteDataType();

protected:
	// String must be zero-terminated when Length is -1.
	virtual void WriteInternal(const char *pStr, int Length = -1) = 0;

public:
	CJsonWriter();
	virtual ~CJsonWriter() = default;

	// The root is created by beginning the first datatype (object, array, value).
	// The writer must not be used after ending the root, which must be unique.

	// Begin writing a new object
	void BeginObject();
	// End current object
	void EndObject();

	// Begin writing a new array
	void BeginArray();
	// End current array
	void EndArray();

	// Write attribute with the given name inside the current object.
	// Names inside one object should be unique, but this is not checked here.
	// Must be used to begin writing anything inside objects and only there.
	// Must be followed by a datatype for the attribute value.
	void WriteAttribute(const char *pName);

	// Functions for writing value literals:
	// - As array values in arrays.
	// - As attribute values after beginning an attribute inside an object.
	// - As root value (only once).
	void WriteStrValue(const char *pValue);
	void WriteIntValue(int Value);
	void WriteBoolValue(bool Value);
	void WriteNullValue();
};

/**
 * Writes JSON to a file.
 */
class CJsonFileWriter : public CJsonWriter
{
	IOHANDLE m_IO;

protected:
	void WriteInternal(const char *pStr, int Length = -1) override;

public:
	/**
	 * Create a new writer object without writing anything to the file yet.
	 * The file will automatically be closed by the destructor.
	 */
	CJsonFileWriter(IOHANDLE IO);
	~CJsonFileWriter();
};

/**
 * Writes JSON to an std::string.
 */
class CJsonStringWriter : public CJsonWriter
{
	std::string m_OutputString;
	bool m_RetrievedOutput = false;

protected:
	void WriteInternal(const char *pStr, int Length = -1) override;

public:
	CJsonStringWriter() = default;
	~CJsonStringWriter() = default;
	std::string &&GetOutputString();
};

#endif
