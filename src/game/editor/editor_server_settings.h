#ifndef GAME_EDITOR_EDITOR_SERVER_SETTINGS_H
#define GAME_EDITOR_EDITOR_SERVER_SETTINGS_H

#include "component.h"
#include "editor_ui.h"

#include <map>
#include <string>
#include <vector>

class CEditor;
struct SMapSettingInt;
struct SMapSettingCommand;
struct IMapSetting;
class CLineInput;

// A parsed map setting argument, storing the name and the type
// Used for validation and to display arguments names
struct SParsedMapSettingArg
{
	char m_aName[32];
	char m_Type;
};

// An argument for the current setting
struct SCurrentSettingArg
{
	char m_aValue[160]; // Value of the argument
	float m_X; // The X position
	size_t m_Start; // Start offset within the input string
	size_t m_End; // End offset within the input string
	bool m_Error; // If the argument is wrong or not
	char m_ExpectedType; // The expected type
};

struct SPossibleValueMatch
{
	const char *m_pValue; // Possible value string
	int m_ArgIndex; // Argument for that possible value
	const void *m_pData; // Generic pointer to pass specific data
};

struct SCommandParseError
{
	char m_aMessage[256];
	int m_ArgIndex;
};

// --------------------------------------
// Builder classes & methods to generate list of possible values
// easily for specific settings and arguments.
// It uses a container stored inside CMapSettingsBackend.
// Usage:
//   CValuesBuilder Builder(&m_Container);
//   // Either do it in one go:
//   Builder("tune").Argument(0).Add("value_1").Add("value_2");
//   // Or reference the builder (useful when using in a loop):
//   auto TuneBuilder = Builder("tune").Argument(0);
//   TuneBuilder.Add("value_1");
//   TuneBuilder.Add("value_2");
//   // ...

using TArgumentValuesList = std::vector<const char *>; // List of possible values
using TSettingValues = std::map<int, TArgumentValuesList>; // Possible values per argument
using TSettingsArgumentValues = std::map<std::string, TSettingValues>; // Possible values per argument, per command/setting name

class CValuesBuilder;
class CSettingValuesBuilder;
class CArgumentValuesListBuilder
{
public:
	CArgumentValuesListBuilder &Add(const char *pString)
	{
		m_pContainer->emplace_back(pString);
		return *this;
	}

private:
	CArgumentValuesListBuilder(std::vector<const char *> *pContainer) :
		m_pContainer(pContainer) {}

	std::vector<const char *> *m_pContainer;
	friend class CSettingValuesBuilder;
};

class CSettingValuesBuilder
{
public:
	CArgumentValuesListBuilder Argument(int Arg) const
	{
		return CArgumentValuesListBuilder(&(*m_pContainer)[Arg]);
	}

private:
	CSettingValuesBuilder(TSettingValues *pContainer) :
		m_pContainer(pContainer) {}

	friend class CValuesBuilder;
	TSettingValues *m_pContainer;
};

class CValuesBuilder
{
public:
	CValuesBuilder(TSettingsArgumentValues *pContainer) :
		m_pContainer(pContainer)
	{
	}

	CSettingValuesBuilder operator()(const char *pSettingName) const
	{
		return CSettingValuesBuilder(&(*m_pContainer)[pSettingName]);
	}

private:
	TSettingsArgumentValues *m_pContainer;
};

// --------------------------------------

struct SValueLoader
{
	static void LoadTuneValues(const CSettingValuesBuilder &TuneBuilder);
	static void LoadTuneZoneValues(const CSettingValuesBuilder &TuneZoneBuilder);
	static void LoadMapBugs(const CSettingValuesBuilder &BugBuilder);
	static void LoadArgumentTuneValues(CArgumentValuesListBuilder &&ArgBuilder);
};

enum class EValidationResult
{
	VALID = 0,
	ERROR,
	INCOMPLETE,
	UNKNOWN,
	OUT_OF_RANGE,
};

enum class ECollisionCheckResult
{
	ERROR,
	REPLACE,
	ADD
};

class CMapSettingsBackend : public CEditorComponent
{
	typedef void (*FLoaderFunction)(const CSettingValuesBuilder &);

public: // General methods
	CMapSettingsBackend() = default;

	void Init(CEditor *pEditor) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnUpdate();

public: // Constraints methods
	enum class EArgConstraint
	{
		DEFAULT = 0,
		UNIQUE,
		MULTIPLE,
	};

	EArgConstraint ArgConstraint(const char *pSettingName, int Arg) const
	{
		return m_ArgConstraintsPerCommand.at(pSettingName).at(Arg);
	}

public: // Backend methods
	const std::vector<SParsedMapSettingArg> &ParsedArgs(const std::shared_ptr<IMapSetting> &pSetting) const
	{
		return m_ParsedCommandArgs.at(pSetting);
	}

public: // CContext
	class CContext
	{
		static const ColorRGBA ms_ArgumentStringColor;
		static const ColorRGBA ms_ArgumentNumberColor;
		static const ColorRGBA ms_ArgumentUnknownColor;
		static const ColorRGBA ms_ErrorColor;

		friend class CMapSettingsBackend;

	public:
		bool CommandIsValid() const { return m_pCurrentSetting != nullptr; }
		int CurrentArg() const { return m_CursorArgIndex; }
		const char *CurrentArgName() const { return (!m_pCurrentSetting || m_CursorArgIndex < 0 || m_CursorArgIndex >= (int)m_pBackend->m_ParsedCommandArgs.at(m_pCurrentSetting).size()) ? nullptr : m_pBackend->m_ParsedCommandArgs.at(m_pCurrentSetting).at(m_CursorArgIndex).m_aName; }
		float CurrentArgPos() const { return m_CursorArgIndex == -1 ? 0 : m_vCurrentArgs[m_CursorArgIndex].m_X; }
		size_t CurrentArgOffset() const { return m_CursorArgIndex == -1 ? 0 : m_vCurrentArgs[m_CursorArgIndex].m_Start; }
		const char *CurrentArgValue() const { return m_CursorArgIndex == -1 ? m_aCommand : m_vCurrentArgs[m_CursorArgIndex].m_aValue; }
		const std::vector<SPossibleValueMatch> &PossibleMatches() const { return m_vPossibleMatches; }
		bool HasError() const { return m_Error.m_aMessage[0] != '\0'; }
		size_t ErrorOffset() const { return m_Error.m_ArgIndex < 0 ? 0 : m_vCurrentArgs.at(m_Error.m_ArgIndex).m_Start; }
		const char *Error() const { return m_Error.m_aMessage; }
		int ArgCount() const { return (int)m_vCurrentArgs.size(); }
		const SCurrentSettingArg &Arg(int Index) const { return m_vCurrentArgs.at(Index); }
		const std::shared_ptr<IMapSetting> &Setting() const { return m_pCurrentSetting; }
		CLineInput *LineInput() const { return m_pLineInput; }

		int CheckCollision(ECollisionCheckResult &Result) const;
		void Update();
		bool UpdateCursor(bool Force = false);
		void Reset();
		void GetCommandHelpText(char *pStr, int Length) const;
		bool Valid() const;
		void ColorArguments(std::vector<STextColorSplit> &vColorSplits) const;

		bool m_AllowUnknownCommands;
		SEditBoxDropdownContext m_DropdownContext;
		int m_CurrentCompletionIndex;

	private: // Methods
		CContext(CMapSettingsBackend *pMaster, CLineInput *pLineinput) :
			m_DropdownContext(), m_pLineInput(pLineinput), m_pBackend(pMaster)
		{
			m_AllowUnknownCommands = false;
			Reset();
		}

		void ClearError();
		EValidationResult ValidateArg(int Index, const char *pArg);
		void UpdatePossibleMatches();
		void ParseArgs(const char *pStr);
		bool OnInput(const IInput::CEvent &Event);
		const char *InputString() const;
		void UpdateCompositionString();

	private: // Fields
		std::shared_ptr<IMapSetting> m_pCurrentSetting; // Current setting, can be nullptr in case of invalid setting name
		std::vector<SCurrentSettingArg> m_vCurrentArgs; // Current parsed arguments from lineinput string
		int m_CursorArgIndex; // The current argument the cursor is over
		std::vector<SPossibleValueMatch> m_vPossibleMatches; // The current matches from cursor argument
		size_t m_LastCursorOffset; // Last cursor offset
		CLineInput *m_pLineInput;
		char m_aCommand[128]; // The current command, not necessarily valid
		SCommandParseError m_Error; // Error

		CMapSettingsBackend *m_pBackend;
		std::string m_CompositionStringBuffer;
	};

	CContext NewContext(CLineInput *pLineInput)
	{
		return CContext(this, pLineInput);
	}

private: // Loader methods
	void LoadAllMapSettings();
	void LoadCommand(const char *pName, const char *pArgs, const char *pHelp);
	void LoadSettingInt(const std::shared_ptr<SMapSettingInt> &pSetting);
	void LoadSettingCommand(const std::shared_ptr<SMapSettingCommand> &pSetting);
	void InitValueLoaders();
	void LoadPossibleValues(const CSettingValuesBuilder &Builder, const std::shared_ptr<IMapSetting> &pSetting);
	void RegisterLoader(const char *pSettingName, const FLoaderFunction &pfnLoader);
	void LoadConstraints();

	static void PossibleConfigVariableCallback(const struct SConfigVariable *pVariable, void *pUserData);

private: // Argument constraints
	using TArgumentConstraints = std::map<int, EArgConstraint>; // Constraint per argument index
	using TCommandArgumentConstraints = std::map<std::string, TArgumentConstraints>; // Constraints per command/setting name

	// Argument constraints builder
	// Used to define arguments constraints for specific commands
	// It uses a container stored in CMapSettingsBackend.
	// Usage:
	//   CCommandArgumentConstraintBuilder Command(&m_Container);
	//   Command("tune", 2).Unique(0); // Defines argument 0 of command "tune" having 2 args as UNIQUE
	//   Command("tune_zone", 3).Multiple(0).Unique(1);
	//   // ^ Multiple() currently is only for readable purposes. It can be omited:
	//   // Command("tune_zone", 3).Unique(1);
	//

	class CCommandArgumentConstraintBuilder;

	class CArgumentConstraintsBuilder
	{
		friend class CCommandArgumentConstraintBuilder;

	private:
		CArgumentConstraintsBuilder(TArgumentConstraints *pContainer) :
			m_pContainer(pContainer){};

		TArgumentConstraints *m_pContainer;

	public:
		CArgumentConstraintsBuilder &Multiple(int Arg)
		{
			// Define a multiple argument constraint
			(*m_pContainer)[Arg] = EArgConstraint::MULTIPLE;
			return *this;
		}

		CArgumentConstraintsBuilder &Unique(int Arg)
		{
			// Define a unique argument constraint
			(*m_pContainer)[Arg] = EArgConstraint::UNIQUE;
			return *this;
		}
	};

	class CCommandArgumentConstraintBuilder
	{
	public:
		CCommandArgumentConstraintBuilder(TCommandArgumentConstraints *pContainer) :
			m_pContainer(pContainer) {}

		CArgumentConstraintsBuilder operator()(const char *pSettingName, int ArgCount)
		{
			for(int i = 0; i < ArgCount; i++)
				(*m_pContainer)[pSettingName][i] = EArgConstraint::DEFAULT;
			return CArgumentConstraintsBuilder(&(*m_pContainer)[pSettingName]);
		}

	private:
		TCommandArgumentConstraints *m_pContainer;
	};

	TCommandArgumentConstraints m_ArgConstraintsPerCommand;

private: // Backend fields
	std::vector<std::shared_ptr<IMapSetting>> m_vpMapSettings;
	std::map<std::shared_ptr<IMapSetting>, std::vector<SParsedMapSettingArg>> m_ParsedCommandArgs; // Parsed available settings arguments, used for validation
	TSettingsArgumentValues m_PossibleValuesPerCommand;
	std::map<std::string, FLoaderFunction> m_LoaderFunctions;

	static CContext *ms_pActiveContext;

	friend class CEditor;
};

#endif
