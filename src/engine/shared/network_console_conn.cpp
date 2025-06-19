/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "network.h"
#include <base/system.h>

void CConsoleNetConnection::Reset()
{
	m_State = NET_CONNSTATE_OFFLINE;
	mem_zero(&m_PeerAddr, sizeof(m_PeerAddr));
	m_aErrorString[0] = 0;

	m_Socket = nullptr;
	m_aBuffer[0] = 0;
	m_BufferOffset = 0;

	m_LineEndingDetected = false;
#if defined(CONF_FAMILY_WINDOWS)
	m_aLineEnding[0] = '\r';
	m_aLineEnding[1] = '\n';
	m_aLineEnding[2] = 0;
#else
	m_aLineEnding[0] = '\n';
	m_aLineEnding[1] = 0;
	m_aLineEnding[2] = 0;
#endif
}

int CConsoleNetConnection::Init(NETSOCKET Socket, const NETADDR *pAddr)
{
	Reset();

	if(net_set_non_blocking(Socket) != 0)
	{
		return -1;
	}

	m_Socket = Socket;
	m_PeerAddr = *pAddr;
	m_State = NET_CONNSTATE_ONLINE;
	return 0;
}

void CConsoleNetConnection::Disconnect(const char *pReason)
{
	if(State() == NET_CONNSTATE_OFFLINE)
		return;

	if(pReason && pReason[0])
		Send(pReason);

	net_tcp_close(m_Socket);

	Reset();
}

int CConsoleNetConnection::Update()
{
	if(State() == NET_CONNSTATE_ONLINE)
	{
		if((int)(sizeof(m_aBuffer)) <= m_BufferOffset)
		{
			m_State = NET_CONNSTATE_ERROR;
			str_copy(m_aErrorString, "too weak connection (out of buffer)");
			return -1;
		}

		int Bytes = net_tcp_recv(m_Socket, m_aBuffer + m_BufferOffset, (int)(sizeof(m_aBuffer)) - m_BufferOffset);

		if(Bytes > 0)
		{
			m_BufferOffset += Bytes;
		}
		else if(Bytes < 0)
		{
			if(net_would_block()) // no data received
				return 0;

			m_State = NET_CONNSTATE_ERROR; // error
			str_copy(m_aErrorString, "connection failure");
			return -1;
		}
		else
		{
			m_State = NET_CONNSTATE_ERROR;
			str_copy(m_aErrorString, "remote end closed the connection");
			return -1;
		}
	}

	return 0;
}

int CConsoleNetConnection::Recv(char *pLine, int MaxLength)
{
	if(State() == NET_CONNSTATE_ONLINE)
	{
		if(m_BufferOffset)
		{
			// find message start
			int StartOffset = 0;
			while(m_aBuffer[StartOffset] == '\r' || m_aBuffer[StartOffset] == '\n')
			{
				// detect clients line ending format
				if(!m_LineEndingDetected)
				{
					m_aLineEnding[0] = m_aBuffer[StartOffset];
					if(StartOffset + 1 < m_BufferOffset && (m_aBuffer[StartOffset + 1] == '\r' || m_aBuffer[StartOffset + 1] == '\n') &&
						m_aBuffer[StartOffset] != m_aBuffer[StartOffset + 1])
						m_aLineEnding[1] = m_aBuffer[StartOffset + 1];
					m_LineEndingDetected = true;
				}

				if(++StartOffset >= m_BufferOffset)
				{
					m_BufferOffset = 0;
					return 0;
				}
			}

			// find message end
			int EndOffset = StartOffset;
			while(m_aBuffer[EndOffset] != '\r' && m_aBuffer[EndOffset] != '\n')
			{
				if(++EndOffset >= m_BufferOffset)
				{
					if(StartOffset > 0)
					{
						mem_move(m_aBuffer, m_aBuffer + StartOffset, m_BufferOffset - StartOffset);
						m_BufferOffset -= StartOffset;
					}
					return 0;
				}
			}

			// extract message and update buffer
			if(MaxLength - 1 < EndOffset - StartOffset)
			{
				if(StartOffset > 0)
				{
					mem_move(m_aBuffer, m_aBuffer + StartOffset, m_BufferOffset - StartOffset);
					m_BufferOffset -= StartOffset;
				}
				return 0;
			}
			mem_copy(pLine, m_aBuffer + StartOffset, EndOffset - StartOffset);
			pLine[EndOffset - StartOffset] = 0;
			str_sanitize_cc(pLine);
			mem_move(m_aBuffer, m_aBuffer + EndOffset, m_BufferOffset - EndOffset);
			m_BufferOffset -= EndOffset;
			return str_utf8_check(pLine);
		}
	}
	return 0;
}

int CConsoleNetConnection::Send(const char *pLine)
{
	if(State() != NET_CONNSTATE_ONLINE)
		return -1;

	char aBuf[1024];
	str_copy(aBuf, pLine, (int)(sizeof(aBuf)) - 2);
	int Length = str_length(aBuf);
	aBuf[Length] = m_aLineEnding[0];
	aBuf[Length + 1] = m_aLineEnding[1];
	aBuf[Length + 2] = m_aLineEnding[2];
	Length += 3;
	const char *pData = aBuf;

	while(true)
	{
		int Send = net_tcp_send(m_Socket, pData, Length);
		if(Send < 0)
		{
			m_State = NET_CONNSTATE_ERROR;
			str_copy(m_aErrorString, "failed to send packet");
			return -1;
		}

		if(Send >= Length)
			break;

		pData += Send;
		Length -= Send;
	}

	return 0;
}
