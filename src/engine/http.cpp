#include "http.h"

#include <base/dbg.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/log.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/version.h>

#include <algorithm>

IHttpRequest::IHttpRequest(const char *pUrl)
{
	str_copy(m_aUrl, pUrl);
	sha256_init(&m_ActualSha256Ctx);
}

IHttpRequest::~IHttpRequest()
{
	dbg_assert(m_File == nullptr, "HTTP request file was not closed");
	free(m_pBody);
	free(m_pBuffer);
	if(m_State == EHttpState::DONE && m_ValidateBeforeOverwrite)
	{
		OnValidation(false);
	}
}

void IHttpRequest::WriteToMemory()
{
	m_WriteToMemory = true;
	m_WriteToFile = false;
}

void IHttpRequest::WriteToFile(IStorage *pStorage, const char *pDest, int StorageType)
{
	m_WriteToMemory = false;
	m_WriteToFile = true;
	str_copy(m_aDest, pDest);
	if(StorageType == -2)
	{
		pStorage->GetBinaryPath(m_aDest, m_aDestAbsolute, sizeof(m_aDestAbsolute));
	}
	else
	{
		pStorage->GetCompletePath(StorageType, m_aDest, m_aDestAbsolute, sizeof(m_aDestAbsolute));
	}
	IStorage::FormatTmpPath(m_aDestAbsoluteTmp, sizeof(m_aDestAbsoluteTmp), m_aDestAbsolute);
}

void IHttpRequest::WriteToFileAndMemory(IStorage *pStorage, const char *pDest, int StorageType)
{
	WriteToFile(pStorage, pDest, StorageType);
	m_WriteToMemory = true;
}

void IHttpRequest::Head()
{
	m_Type = REQUEST::HEAD;
}

void IHttpRequest::Post(const unsigned char *pData, size_t DataLength)
{
	m_Type = REQUEST::POST;
	m_BodyLength = DataLength;
	m_pBody = (unsigned char *)malloc(std::max((size_t)1, DataLength));
	mem_copy(m_pBody, pData, DataLength);
}

void IHttpRequest::PostJson(const char *pJson)
{
	m_Type = REQUEST::POST_JSON;
	m_BodyLength = str_length(pJson);
	m_pBody = (unsigned char *)malloc(m_BodyLength);
	mem_copy(m_pBody, pJson, m_BodyLength);
}

void IHttpRequest::HeaderString(const char *pName, const char *pValue)
{
	char aHeader[256];
	str_format(aHeader, sizeof(aHeader), "%s: %s", pName, pValue);
	Header(aHeader);
}

void IHttpRequest::HeaderInt(const char *pName, int Value)
{
	char aHeader[256];
	str_format(aHeader, sizeof(aHeader), "%s: %d", pName, Value);
	Header(aHeader);
}

const char *IHttpRequest::Dest() const
{
	if(m_WriteToFile)
	{
		return m_aDest;
	}
	else
	{
		return nullptr;
	}
}

void IHttpRequest::Wait()
{
	std::unique_lock Lock(m_WaitMutex);
	m_WaitCondition.wait(Lock, [this]() {
		EHttpState State = m_State.load(std::memory_order_seq_cst);
		return State != EHttpState::QUEUED && State != EHttpState::RUNNING;
	});
}

void IHttpRequest::OnValidation(bool Success)
{
	dbg_assert(m_ValidateBeforeOverwrite, "this function is illegal to call without having set ValidateBeforeOverwrite");
	m_ValidateBeforeOverwrite = false;
	if(Success)
	{
		if(m_IfModifiedSince >= 0 && m_StatusCode == 304) // 304 Not Modified
		{
			(void)fs_remove(m_aDestAbsoluteTmp);
			return;
		}
		if(fs_rename(m_aDestAbsoluteTmp, m_aDestAbsolute))
		{
			log_error("http", "i/o error, cannot move file: %s", m_aDest);
			m_State = EHttpState::ERROR;
			(void)fs_remove(m_aDestAbsoluteTmp);
		}
	}
	else
	{
		m_State = EHttpState::ERROR;
		(void)fs_remove(m_aDestAbsoluteTmp);
	}
}

void IHttpRequest::Result(unsigned char **ppResult, size_t *pResultLength) const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	dbg_assert(m_WriteToMemory, "Result only usable when written to memory");
	*ppResult = m_pBuffer;
	*pResultLength = m_ResponseLength;
}

json_value *IHttpRequest::ResultJson() const
{
	unsigned char *pResult;
	size_t ResultLength;
	Result(&pResult, &ResultLength);
	return JsonParse((char *)pResult, ResultLength);
}

const SHA256_DIGEST &IHttpRequest::ResultSha256() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	dbg_assert(m_ActualSha256.has_value(), "Result SHA256 missing");
	return m_ActualSha256.value();
}

int IHttpRequest::StatusCode() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	return m_StatusCode;
}

std::optional<int64_t> IHttpRequest::ResultAgeSeconds() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	if(!m_ResultDate || !m_ResultLastModified)
	{
		return {};
	}
	return *m_ResultDate - *m_ResultLastModified;
}

std::optional<int64_t> IHttpRequest::ResultLastModified() const
{
	dbg_assert(State() == EHttpState::DONE, "Request not done");
	return m_ResultLastModified;
}

static bool CalculateSha256(const char *pAbsoluteFilename, SHA256_DIGEST *pSha256)
{
	IOHANDLE File = io_open(pAbsoluteFilename, IOFLAG_READ);
	if(!File)
	{
		return false;
	}
	SHA256_CTX Sha256Ctxt;
	sha256_init(&Sha256Ctxt);
	unsigned char aBuffer[64 * 1024];
	while(true)
	{
		unsigned Bytes = io_read(File, aBuffer, sizeof(aBuffer));
		if(Bytes == 0)
			break;
		sha256_update(&Sha256Ctxt, aBuffer, Bytes);
	}
	io_close(File);
	*pSha256 = sha256_finish(&Sha256Ctxt);
	return true;
}

const char *const IHttpRequest::USER_AGENT_STRING = GAME_NAME " " GAME_RELEASE_VERSION " (" CONF_PLATFORM_STRING "; " CONF_ARCH_STRING ")";

const char *IHttpRequest::GetRequestType(REQUEST Type)
{
	switch(Type)
	{
	case REQUEST::GET:
		return "GET";
	case REQUEST::HEAD:
		return "HEAD";
	case REQUEST::POST:
	case REQUEST::POST_JSON:
		return "POST";
	default:
		dbg_assert_failed("Invalid Type: %d", (int)Type);
	}
}

bool IHttpRequest::ShouldSkipRequest()
{
	if(m_WriteToFile && m_ExpectedSha256.has_value())
	{
		SHA256_DIGEST Sha256;
		if(CalculateSha256(m_aDestAbsolute, &Sha256) && Sha256 == m_ExpectedSha256.value())
		{
			log_debug("http", "skipping download because expected file already exists: %s", m_aDest);
			return true;
		}
	}
	return false;
}

bool IHttpRequest::BeforeInit()
{
	if(m_WriteToFile)
	{
		if(m_SkipByFileTime)
		{
			time_t FileCreatedTime, FileModifiedTime;
			if(fs_file_time(m_aDestAbsolute, &FileCreatedTime, &FileModifiedTime) == 0)
			{
				m_IfModifiedSince = FileModifiedTime;
			}
		}

		if(fs_makedir_rec_for(m_aDestAbsoluteTmp) < 0)
		{
			log_error("http", "i/o error, cannot create folder for: %s", m_aDest);
			return false;
		}

		m_File = io_open(m_aDestAbsoluteTmp, IOFLAG_WRITE);
		if(!m_File)
		{
			log_error("http", "i/o error, cannot open file: %s", m_aDest);
			return false;
		}
	}
	return true;
}

size_t IHttpRequest::OnData(const char *pData, size_t DataSize)
{
	// Need to check for the maximum response size here as implementation may not support it,
	// e.g. curl can only guarantee it if the server sets a Content-Length header.
	if(m_MaxResponseSize >= 0 && m_ResponseLength + DataSize > (uint64_t)m_MaxResponseSize)
	{
		return 0;
	}

	if(DataSize == 0)
	{
		return DataSize;
	}

	sha256_update(&m_ActualSha256Ctx, pData, DataSize);

	size_t Result = DataSize;

	if(m_WriteToMemory)
	{
		size_t NewBufferSize = std::max((size_t)1024, m_BufferSize);
		while(m_ResponseLength + DataSize > NewBufferSize)
		{
			NewBufferSize *= 2;
		}
		if(NewBufferSize != m_BufferSize)
		{
			m_pBuffer = (unsigned char *)realloc(m_pBuffer, NewBufferSize);
			m_BufferSize = NewBufferSize;
		}
		mem_copy(m_pBuffer + m_ResponseLength, pData, DataSize);
	}
	if(m_WriteToFile)
	{
		Result = io_write(m_File, pData, DataSize);
	}
	m_ResponseLength += DataSize;
	return Result;
}

void IHttpRequest::OnCompletionInternal(EHttpState State)
{
	if(State == EHttpState::DONE)
	{
		m_ActualSha256 = sha256_finish(&m_ActualSha256Ctx);
		if(m_ExpectedSha256.has_value() && m_ActualSha256.value() != m_ExpectedSha256.value())
		{
			if(g_Config.m_DbgHttp || m_LogProgress >= HTTPLOG::FAILURE)
			{
				char aActualSha256[SHA256_MAXSTRSIZE];
				sha256_str(m_ActualSha256.value(), aActualSha256, sizeof(aActualSha256));
				char aExpectedSha256[SHA256_MAXSTRSIZE];
				sha256_str(m_ExpectedSha256.value(), aExpectedSha256, sizeof(aExpectedSha256));
				log_error("http", "SHA256 mismatch: got=%s, expected=%s, url=%s", aActualSha256, aExpectedSha256, m_aUrl);
			}
			State = EHttpState::ERROR;
		}
	}

	if(m_WriteToFile)
	{
		if(m_File && io_close(m_File) != 0)
		{
			log_error("http", "i/o error, cannot close file: %s", m_aDest);
			State = EHttpState::ERROR;
		}
		m_File = nullptr;

		if(State == EHttpState::ERROR || State == EHttpState::ABORTED)
		{
			(void)fs_remove(m_aDestAbsoluteTmp);
		}
		else if(m_IfModifiedSince >= 0 && m_StatusCode == 304) // 304 Not Modified
		{
			(void)fs_remove(m_aDestAbsoluteTmp);
			if(m_WriteToMemory)
			{
				free(m_pBuffer);
				m_pBuffer = nullptr;
				m_ResponseLength = 0;
				void *pBuffer;
				unsigned Length;
				IOHANDLE File = io_open(m_aDestAbsolute, IOFLAG_READ);
				bool Success = File && io_read_all(File, &pBuffer, &Length);
				if(File)
				{
					io_close(File);
				}
				if(Success)
				{
					m_pBuffer = (unsigned char *)pBuffer;
					m_ResponseLength = Length;
				}
				else
				{
					log_error("http", "i/o error, cannot read existing file: %s", m_aDest);
					State = EHttpState::ERROR;
				}
			}
		}
		else if(!m_ValidateBeforeOverwrite)
		{
			if(fs_rename(m_aDestAbsoluteTmp, m_aDestAbsolute))
			{
				log_error("http", "i/o error, cannot move file: %s", m_aDest);
				State = EHttpState::ERROR;
				(void)fs_remove(m_aDestAbsoluteTmp);
			}
		}
	}

	// The globally visible state must be updated after OnCompletion has finished,
	// or other threads may try to access the result of a completed HTTP request,
	// before the result has been initialized/updated in OnCompletion.
	if(m_pProgressCallback != nullptr)
	{
		m_pProgressCallback->OnCompletion(State);
	}
	{
		std::unique_lock WaitLock(m_WaitMutex);
		m_State = State;
	}
	m_WaitCondition.notify_all();
}
