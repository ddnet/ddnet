/*
#include "censor.h"

#include <base/log.h>

#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/config.h>

#include <optional>
#include <utility>

static void ReplaceWords(char *pBuffer, const std::vector<std::string> &vWords, char Replacement)
{
	if(!pBuffer)
		return;

	for(const auto &Word : vWords)
	{
		const char *pEnd = nullptr;
		const char *pStart = pBuffer;

		while(pStart)
		{
			pStart = str_utf8_find_nocase(pStart, Word.c_str(), &pEnd);
			if(!pStart)
				continue;
			if((pStart == pBuffer || str_utf8_isspace(*(pStart - 1))) && (str_utf8_isspace(*(pEnd))))
			{
				while(pStart != pEnd)
				{
					pBuffer[pStart - pBuffer] = Replacement;
					pStart++;
				}
			}
			pStart = pEnd;
		}
	}
}

void CCensor::ConchainRefreshCensorList(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(!g_Config.m_ClCensorChat)
		log_warn("censor", "The config cl_censor_chat is currently off! You have to set it to 1 to use the censor feature.");
	if(pResult->NumArguments() && str_comp(g_Config.m_ClCensorUrl, pResult->GetString(1)))
		((CCensor *)pUserData)->Reset();
}

CCensor::CCensor()
{
	m_vCensoredWords = {};
}

void CCensor::OnInit()
{
	Reset();
}

void CCensor::OnConsoleInit()
{
	Console()->Chain("cl_censor_url", ConchainRefreshCensorList, this);
}

void CCensor::Reset()
{
	m_vCensoredWords.clear();

	if(m_pCensorListDownloadJob)
	{
		m_pCensorListDownloadJob->Abort();
	}

	m_pCensorListDownloadJob = std::make_shared<CCensorListDownloadJob>(this, g_Config.m_ClCensorUrl, "censored_words_online.json");
	Engine()->AddJob(m_pCensorListDownloadJob);
}

void CCensor::OnRender()
{
	if(m_pCensorListDownloadJob && m_pCensorListDownloadJob->Done())
	{
		if(m_pCensorListDownloadJob->m_vLoadedWords)
			m_vCensoredWords = std::move(*m_pCensorListDownloadJob->m_vLoadedWords);
		m_pCensorListDownloadJob = nullptr;
	}
}

void CCensor::OnShutdown()
{
	if(m_pCensorListDownloadJob)
	{
		m_pCensorListDownloadJob->Abort();
	}
}

void CCensor::CensorMessage(char *pMessage) const
{
	if(!g_Config.m_ClCensorChat)
		return;

	if(!*pMessage)
		return;
	ReplaceWords(pMessage, m_vCensoredWords, '*');
}

std::optional<std::vector<std::string>> CCensor::LoadCensorListFromFile(const char *pFilePath) const
{
	void *pFileData;
	unsigned FileSize;
	if(!Storage()->ReadFile(pFilePath, IStorage::TYPE_SAVE, &pFileData, &FileSize))
	{
		log_error("censor", "Failed to open/read word censor file '%s'", pFilePath);
		return std::nullopt;
	}

	std::optional<std::vector<std::string>> vWordList = LoadCensorList(pFileData, FileSize);
	free(pFileData);
	return vWordList;
}

std::optional<std::vector<std::string>> CCensor::LoadCensorList(const void *pListText, size_t ListTextLen) const
{
	std::vector<std::string> vWordList = {};

	json_value *pData = nullptr;
	json_settings JsonSettings{};
	char aError[256];
	pData = json_parse_ex(&JsonSettings, static_cast<const json_char *>(pListText), ListTextLen, aError);

	if(!pData)
	{
		log_error("censor", "Failed to parse censor list: %s", aError);
		return std::nullopt;
	}

	if(pData->type != json_object)
	{
		log_error("censor", "Censor list malformed: root must be an object");
		return std::nullopt;
	}

	const json_value &Words = (*pData)["words"];

	if(Words.type != json_array)
	{
		log_error("censor", "Censor list malformed: 'words' must be an array");
		return std::nullopt;
	}
	int Length = Words.u.array.length;

	for(int i = 0; i < Length; ++i)
	{
		const json_value &JsonWord = Words[i];

		if(JsonWord.type != json_string)
		{
			log_error("censor", "Censor list malformed: 'words' must be an array of strings (error at index %d)", i);
			return std::nullopt;
		}

		const char *pWord = JsonWord.u.string.ptr;
		if(*pWord)
		{
			vWordList.emplace_back(pWord);
		}
	}

	json_value_free(pData);

	log_info("censor", "Loaded %" PRIzu " words from censor list", vWordList.size());
	return vWordList;
}

CCensor::CCensorListDownloadJob::CCensorListDownloadJob(CCensor *pCensor, const char *pUrl, const char *pSaveFilePath) :
	m_pCensor(pCensor)
{
	str_copy(m_aUrl, pUrl);
	str_copy(m_aSaveFilePath, pSaveFilePath);
	m_vLoadedWords = std::nullopt;
	Abortable(true);
}

bool CCensor::CCensorListDownloadJob::Abort()
{
	if(!IJob::Abort())
	{
		return false;
	}

	const CLockScope LockScope(m_Lock);
	if(m_pGetRequest)
	{
		m_pGetRequest->Abort();
		m_pGetRequest = nullptr;
	}
	return true;
}

void CCensor::CCensorListDownloadJob::Run()
{
	const CTimeout Timeout{10000, 0, 8192, 10};
	const size_t MaxResponseSize = 50 * 1024 * 1024; // 50 MiB

	std::shared_ptr<CHttpRequest> pGet = HttpGetBoth(m_aUrl, m_pCensor->Storage(), m_aSaveFilePath, IStorage::TYPE_SAVE);
	pGet->Timeout(Timeout);
	pGet->MaxResponseSize(MaxResponseSize);
	pGet->SkipByFileTime(true);
	pGet->ValidateBeforeOverwrite(true);
	pGet->FailOnErrorStatus(false);
	pGet->LogProgress(HTTPLOG::ALL);
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = pGet;
	}
	m_pCensor->Http()->Run(pGet);

	// Load existing file while waiting for the HTTP request
	{
		m_vLoadedWords = m_pCensor->LoadCensorListFromFile(m_aSaveFilePath);
	}

	pGet->Wait();
	{
		const CLockScope LockScope(m_Lock);
		m_pGetRequest = nullptr;
	}
	if(pGet->State() != EHttpState::DONE || State() == IJob::STATE_ABORTED)
	{
		return;
	}
	if(pGet->StatusCode() >= 400)
	{
		log_info("censor", "got http error %d", pGet->StatusCode());
		return;
	}

	if(pGet->StatusCode() == 304) // 304 Not Modified
	{
		return;
	}

	if(State() == IJob::STATE_ABORTED)
	{
		return;
	}

	unsigned char *pResult;
	size_t ResultSize;
	pGet->Result(&pResult, &ResultSize);

	std::optional<std::vector<std::string>> vLoadedWords = m_pCensor->LoadCensorList((const char *)pResult, ResultSize);
	bool Success = vLoadedWords.has_value();
	pGet->OnValidation(Success);
	if(Success)
	{
		m_vLoadedWords = std::move(vLoadedWords);
	}
}
*/
