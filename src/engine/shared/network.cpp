/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/log.h>
#include <base/system.h>
#include <net/net.h>

#include "config.h"
#include "huffman.h"
#include "network.h"

CHuffman CNetBase::ms_Huffman;

void CNetBase::OpenLog(IOHANDLE DataLogSent, IOHANDLE DataLogRecv)
{
	// unimplemented
	io_close(DataLogSent);
	io_close(DataLogRecv);
}

void CNetBase::CloseLog()
{
	// unimplemented
}

int CNetBase::Compress(const void *pData, int DataSize, void *pOutput, int OutputSize)
{
	return ms_Huffman.Compress(pData, DataSize, pOutput, OutputSize);
}

int CNetBase::Decompress(const void *pData, int DataSize, void *pOutput, int OutputSize)
{
	return ms_Huffman.Decompress(pData, DataSize, pOutput, OutputSize);
}

void NetLogger(int Level, const char *pSystem, size_t SystemLen, const char *pMessage, size_t MessageLen)
{
	char aSystem[64];
	str_truncate(aSystem, sizeof(aSystem), pSystem, SystemLen);
	log_log((LEVEL)Level, aSystem, "%.*s", (int)MessageLen, pMessage);
}

void CNetBase::Init()
{
	ms_Huffman.Init();
	ddnet_net_set_logger(NetLogger);
}
