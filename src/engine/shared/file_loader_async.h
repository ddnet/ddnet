// #ifndef FILE_LOADER_ASYNC_H
// #define FILE_LOADER_ASYNC_H

// #include "file_loader.h"
// #include "engine/shared/uuid_manager.h"
// #include <mutex>
// #include <utility>

// class CMassFileLoaderAsync : public CMassFileLoader
//{
// public:
//	CMassFileLoaderAsync(IStorage *pStorage, uint8_t Flags = LOAD_FLAGS_NONE);
//	virtual ~CMassFileLoaderAsync();

//	template<typename T, typename... Ts>
//	void SetPaths(T Path, Ts... Paths)
//	{
//		m_RequestedPaths.push_back(std::string(Path));
//		(SetPaths(std::forward<T>(Paths)), ...);
//	}
//	void SetFileExtension(const std::string &Extension) override;
//	void SetFileLoadedCallback(std::function<FileLoadedCallbackSignature> Function) override { m_fnFileLoadedCallback = Function; }
//	void SetLoadFailedCallback(std::function<LoadFailedCallbackSignature> Function) override { m_fnLoadFailedCallback = Function; }
//	bool IsFinished() { return m_Finished; } // Is ready for deletion

//	// If this is implemented, it is expected that the object is deleted by the caller.
//	using LoadFinishedCallbackSignature = void(unsigned int Count, void *pUser);
//	void SetLoadFinishedCallback(std::function<LoadFinishedCallbackSignature> Function) { m_fnLoadFinishedCallback = std::move(Function); }

//	std::optional<unsigned int> BeginLoad();

// private:
//	//	static void Load(void *pUser);

//	CUuid ThreadId;

//	bool m_Continue = true, m_Finished = false;
//	std::function<FileLoadedCallbackSignature> m_fnFileLoadedCallback;
//	std::function<LoadFailedCallbackSignature> m_fnLoadFailedCallback;
//	std::function<LoadFinishedCallbackSignature> m_fnLoadFinishedCallback;

//	std::vector<std::string> m_RequestedPaths;
//	std::unordered_map<std::string, std::vector<std::string> *> m_PathCollection; // oh geez
//	std::string m_Extension;

//	IStorage *m_pStorage = nullptr;

//	using FileIndex = std::vector<std::string>;
//	struct SListDirectoryCallbackUserInfo
//	{
//		std::string *m_pCurrentDirectory;
//		CMassFileLoader *m_pThis;
//		bool *m_pContinue;
//	};
//};

// #endif // FILE_LOADER_ASYNC_H
