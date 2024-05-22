/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "jsonwriter.h"

#include <base/system.h>

static char EscapeJsonChar(char c)
{
	switch(c)
	{
	case '\"': return '\"';
	case '\\': return '\\';
	case '\b': return 'b';
	case '\n': return 'n';
	case '\r': return 'r';
	case '\t': return 't';
	// Don't escape '\f', who uses that. :)
	default: return 0;
	}
}

CJsonWriter::CJsonWriter()
{
	m_Indentation = 0;
}

void CJsonWriter::BeginObject()
{
	dbg_assert(CanWriteDatatype(), "Cannot write object here");
	WriteIndent(false);
	WriteInternal("{");
	PushState(STATE_OBJECT);
}

void CJsonWriter::EndObject()
{
	dbg_assert(TopState()->m_Kind == STATE_OBJECT, "Cannot end object here");
	PopState();
	CompleteDataType();
	WriteIndent(true);
	WriteInternal("}");
}

void CJsonWriter::BeginArray()
{
	dbg_assert(CanWriteDatatype(), "Cannot write array here");
	WriteIndent(false);
	WriteInternal("[");
	PushState(STATE_ARRAY);
}

void CJsonWriter::EndArray()
{
	dbg_assert(TopState()->m_Kind == STATE_ARRAY, "Cannot end array here");
	PopState();
	CompleteDataType();
	WriteIndent(true);
	WriteInternal("]");
}

void CJsonWriter::WriteAttribute(const char *pName)
{
	dbg_assert(TopState()->m_Kind == STATE_OBJECT, "Cannot write attribute here");
	WriteIndent(false);
	WriteInternalEscaped(pName);
	WriteInternal(": ");
	PushState(STATE_ATTRIBUTE);
}

void CJsonWriter::WriteStrValue(const char *pValue)
{
	dbg_assert(CanWriteDatatype(), "Cannot write value here");
	WriteIndent(false);
	WriteInternalEscaped(pValue);
	CompleteDataType();
}

void CJsonWriter::WriteIntValue(int Value)
{
	dbg_assert(CanWriteDatatype(), "Cannot write value here");
	WriteIndent(false);
	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%d", Value);
	WriteInternal(aBuf);
	CompleteDataType();
}

void CJsonWriter::WriteBoolValue(bool Value)
{
	dbg_assert(CanWriteDatatype(), "Cannot write value here");
	WriteIndent(false);
	WriteInternal(Value ? "true" : "false");
	CompleteDataType();
}

void CJsonWriter::WriteNullValue()
{
	dbg_assert(CanWriteDatatype(), "Cannot write value here");
	WriteIndent(false);
	WriteInternal("null");
	CompleteDataType();
}

bool CJsonWriter::CanWriteDatatype()
{
	return m_States.empty() || TopState()->m_Kind == STATE_ARRAY || TopState()->m_Kind == STATE_ATTRIBUTE;
}

void CJsonWriter::WriteInternalEscaped(const char *pStr)
{
	WriteInternal("\"");
	int UnwrittenFrom = 0;
	int Length = str_length(pStr);
	for(int i = 0; i < Length; i++)
	{
		char SimpleEscape = EscapeJsonChar(pStr[i]);
		// Assuming ASCII/UTF-8, exactly everything below 0x20 is a
		// control character.
		bool NeedsEscape = SimpleEscape || (unsigned char)pStr[i] < 0x20;
		if(NeedsEscape)
		{
			if(i - UnwrittenFrom > 0)
			{
				WriteInternal(pStr + UnwrittenFrom, i - UnwrittenFrom);
			}

			if(SimpleEscape)
			{
				char aStr[2];
				aStr[0] = '\\';
				aStr[1] = SimpleEscape;
				WriteInternal(aStr, sizeof(aStr));
			}
			else
			{
				char aStr[7];
				str_format(aStr, sizeof(aStr), "\\u%04x", pStr[i]);
				WriteInternal(aStr);
			}
			UnwrittenFrom = i + 1;
		}
	}
	if(Length - UnwrittenFrom > 0)
	{
		WriteInternal(pStr + UnwrittenFrom, Length - UnwrittenFrom);
	}
	WriteInternal("\"");
}

void CJsonWriter::WriteIndent(bool EndElement)
{
	const bool NotRootOrAttribute = !m_States.empty() && TopState()->m_Kind != STATE_ATTRIBUTE;

	if(NotRootOrAttribute && !TopState()->m_Empty && !EndElement)
		WriteInternal(",");

	if(NotRootOrAttribute || EndElement)
		WriteInternal("\n");

	if(NotRootOrAttribute)
		for(int i = 0; i < m_Indentation; i++)
			WriteInternal("\t");
}

void CJsonWriter::PushState(EJsonStateKind NewState)
{
	if(!m_States.empty())
	{
		m_States.top().m_Empty = false;
	}
	m_States.emplace(NewState);
	if(NewState != STATE_ATTRIBUTE)
	{
		m_Indentation++;
	}
}

CJsonWriter::SState *CJsonWriter::TopState()
{
	dbg_assert(!m_States.empty(), "json stack is empty");
	return &m_States.top();
}

CJsonWriter::EJsonStateKind CJsonWriter::PopState()
{
	dbg_assert(!m_States.empty(), "json stack is empty");
	SState TopState = m_States.top();
	m_States.pop();
	if(TopState.m_Kind != STATE_ATTRIBUTE)
	{
		m_Indentation--;
	}
	return TopState.m_Kind;
}

void CJsonWriter::CompleteDataType()
{
	if(!m_States.empty() && TopState()->m_Kind == STATE_ATTRIBUTE)
		PopState(); // automatically complete the attribute

	if(!m_States.empty())
		TopState()->m_Empty = false;
}

CJsonFileWriter::CJsonFileWriter(IOHANDLE IO)
{
	dbg_assert((bool)IO, "IO handle invalid");
	m_IO = IO;
}

CJsonFileWriter::~CJsonFileWriter()
{
	// Ensure newline at the end
	WriteInternal("\n");
	io_close(m_IO);
}

void CJsonFileWriter::WriteInternal(const char *pStr, int Length)
{
	io_write(m_IO, pStr, Length < 0 ? str_length(pStr) : Length);
}

void CJsonStringWriter::WriteInternal(const char *pStr, int Length)
{
	dbg_assert(!m_RetrievedOutput, "Writer output has already been retrieved");
	m_OutputString += Length < 0 ? pStr : std::string(pStr, Length);
}

std::string &&CJsonStringWriter::GetOutputString()
{
	// Ensure newline at the end. Modify member variable so we can move it when returning.
	WriteInternal("\n");
	m_RetrievedOutput = true; // prevent further usage of this writer
	return std::move(m_OutputString);
}
