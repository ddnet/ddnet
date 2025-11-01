# Contributing code to DDNet

## General

Please open an issue first discussing the idea before starting to write code.
It would be unfortunate if you spend time working on a contribution that does not align with the ideals of the DDNet project.

A non-exhaustive list of things that usually get rejected:
- Extending dummy with new gameplay-affecting features.
  https://github.com/ddnet/ddnet/pull/8275
  https://github.com/ddnet/ddnet/pull/5443#issuecomment-1158437505
- Breaking backwards compatibility in the network protocol or file formats such as skins and demos.
- Breaking backwards compatibility in gameplay:
	- Existing ranks should not be made impossible.
	- Existing maps should not break.
	- New gameplay should not make runs easier on already completed maps.

Check the [list of issues](https://github.com/ddnet/ddnet/issues) to find issues to work on.
Unlabeled issues have not been triaged yet and are usually not good candidates.
Furthermore, the label https://github.com/ddnet/ddnet/labels/needs-discussion indicates issues that still need discussion before they can be implemented and issues with the label https://github.com/ddnet/ddnet/labels/fix-changes-physics are too involved for new contributors.
Working on issues with the labels https://github.com/ddnet/ddnet/labels/good%20first%20issue, https://github.com/ddnet/ddnet/labels/bug and https://github.com/ddnet/ddnet/labels/feature-accepted is recommended.
Make sure the issue is not already being worked on by someone else, by checking its assignment and whether there are open pull requests linked to it.
If you would like to work on an issue, please comment on it to be assigned to it or if you have any questions.

Adding new features generally requires the support of at least two maintainers to avoid feature creep.

## Programming languages

We currently use the following languages to develop DDNet.

- C++
- very marginally Rust
- Python for code generation and supporting tools
- CMake for building

Adding code in another programming language is not possible.

For platform support, we also use other programming languages like Java on
Android or Objective-C++ on macOS, but this is confined to platform-specific
code.

## Code style

There are a few style rules. Some of them are enforced by CI and some of them are manually checked by reviewers.
If your github pipeline shows some errors please have a look at the logs and try to fix them.

Such fix commits should ideally be squashed into one big commit using `git commit --amend` or `git rebase -i`.

A lot of the style offenses can be fixed automatically by running the fix script `./scripts/fix_style.py`.

We use clang-format 10 to format C++ code. If your package manager no longer provides this version, you can download it from https://pypi.org/project/clang-format/10.0.1.1/.

### Upper camel case for variables, methods, class names

With the exception of files in the `src/base` folder.

For single words

- `int length = 0;` ❌
- `int Length = 0;` ✅

For multiple words:

- `int maxLength = 0;` ❌
- `int MaxLength = 0;` ✅

### Variable names should be descriptive

❌ Avoid:

```cpp
for(int i = 0; i < MAX_CLIENTS; i++)
{
	for(int k = 0; k < NUM_DUMMIES; k++)
	{
		if(k == 0)
			continue;

		m_aClients[i].Foo();
	}
}
```

✅ Instead do:

```cpp
for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		if(Dummy == 0)
			continue;

		m_aClients[ClientId].Foo();
	}
}
```

More examples can be found [here](https://github.com/ddnet/ddnet/pull/8288#issuecomment-2094097306).

### Our interpretation of the hungarian notation

DDNet inherited the hungarian notation like prefixes from [Teeworlds](https://www.teeworlds.com/?page=docs&wiki=nomenclature).

Only use the prefixes listed below. The DDNet code base does **NOT** follow the whole hungarian notation strictly.

Do **NOT** use `c` for constants or `b` for booleans or `i` for integers.

C-style function pointers are pointers, but `std::function` are not.

#### For variables

| Prefix | Usage | Example |
| --- | --- | --- |
| `m_` | Class member | `int m_Mode`, `CLine m_aLines[]` |
| `g_` | Global member | `CConfig g_Config` |
| `s_` | Static variable | `static EHistoryType s_HistoryType`, `static char *ms_apSkinNameVariables[NUM_DUMMIES]` |
| `p` | Both raw and smart pointers | `char *pName`, `void **ppUserData`, `std::unique_ptr<IStorage> pStorage` |
| `a` | Fixed sized arrays and `std::array`s | `float aWeaponInitialOffset[NUM_WEAPONS]`, `std::array<char, 12> aOriginalData` |
| `v` | Vectors (`std::vector`) | `std::vector<CLanguage> m_vLanguages` |
| `pfn` | Function pointers (NOT `std::function`) | `m_pfnUnknownCommandCallback = pfnCallback` |
| `F` | Function type definitions | `typedef void (*FCommandCallback)(IResult *pResult, void *pUserData)`, `typedef std::function<int()> FButtonColorCallback` |

Combine these appropriately.

#### For classes

| Prefix | Usage | Example |
| --- | --- | --- |
| `C` | Classes | `class CTextCursor` |
| `I` | Interfaces | `class IFavorites` |
| `S` | ~~Structs (Use classes instead)~~ | ~~`struct STextContainerUsages`~~ |

### Enumerations

Both unscoped enums (`enum`) and scoped enums (`enum class`) should start with `E` and be CamelCase. The literals should use SCREAMING_SNAKE_CASE.

All new code should use scoped enums where the names of the literals should not contain the enum name.

❌ Avoid:

```cpp
enum STATUS
{
	STATUS_PENDING,
	STATUS_OKAY,
	STATUS_ERROR,
};
```

✅ Instead do:

```cpp
enum class EStatus
{
	PENDING,
	OKAY,
	ERROR,
};
```

Also do **not** use `enum` or `enum class` for flags. See the next section instead.
`enum class` should only be used for enumerations. Constants with a typed value should use `constexpr` instead.

### Bitwise flags

The current code uses a lot of `enum` for that. New code should use `inline constexpr` for bitwise flags.

❌ Avoid:

```cpp
enum
{
	CFGFLAG_SAVE = 1,
	CFGFLAG_CLIENT = 2,
	CFGFLAG_SERVER = 4,
	CFGFLAG_STORE = 8,
	CFGFLAG_MASTER = 16,
	CFGFLAG_ECON = 32,
};
```

✅ Instead do:

```cpp
namespace ConfigFlag
{
	inline constexpr uint32_t SAVE = 1 << 0;
	inline constexpr uint32_t CLIENT = 1 << 1;
	inline constexpr uint32_t SERVER = 1 << 2;
	inline constexpr uint32_t STORE = 1 << 3;
	inline constexpr uint32_t MASTER = 1 << 4;
	inline constexpr uint32_t ECON = 1 << 5;
}
```

### Using the C preprocessor should be avoided

- Avoid `#define` directives for constants. Use `enum class` or `inline constexpr` instead.
  Only use `#define` directives if there is no other option (e.g. for conditional compilation).
- Avoid function-like `#define` directives. If possible, extract code into functions or lambda expressions instead of macros.
   Only use function-like `#define` directives if there is no other, more readable option.

### The usage of `goto` is not encouraged

Do not use the `goto` keyword in new code, there are better control flow constructs in C++.

### Assignments in if statements should be avoided

Do not set variables in if statements.

❌

```cpp
int Foo;
if((Foo = 2)) { .. }
```

✅

```cpp
int Foo = 2;
if(Foo) { .. }
```

Unless the alternative code is more complex and harder to read.

### Using integers in boolean contexts should be avoided

❌

```cpp
int Foo = 0;
if(!Foo) { .. }
```

✅

```cpp
int Foo = 0;
if(Foo != 0) { .. }
```

### Methods with default arguments should be avoided

Default arguments tend to break quickly, if you have multiple you have to specify each even if you only want to change the last one.

### Method overloading should be avoided

Try finding descriptive names instead.

### Global/static variables should be avoided

Use member variables or pass state by parameter instead of using global or static variables since static variables share the same value across instances of a class.

Avoid static variables ❌: 

```cpp
int CMyClass::Foo()
{
	static int s_Count = 0;
	s_Count++;
	return s_Count;
}
```

Use member variables instead ✅:

```cpp
class CMyClass
{
	int m_Count = 0;
};
int CMyClass::Foo()
{
	m_Count++;
	return m_Count;
}
```

Constants can be static ✅:

```cpp
static constexpr int ANSWER = 42;
```

### Getters should not have a `Get` prefix

While the code base already has a lot of methods that start with a `Get` prefix. If new getters are added they should not contain a prefix.

❌

```cpp
int GetMyVariable() { return m_MyVariable; }
```

✅

```cpp
int MyVariable() { return m_MyVariable; }
```

### Class member variables should be initialized where they are declared

Instead of doing this ❌:

```cpp
class CFoo
{
	int m_Foo;
};
```

Do this instead if possible ✅:

```cpp
class CFoo
{
	int m_Foo = 0;
};
```

### The usage of `class` should be favored over `struct`

While there are still many `struct`s being used in the code, all new code should only use `class`es.

### Modern C++ should be used instead of old C styled code

DDNet balances in being portable (easy to compile on all common distributions) and using modern features.
So you are encouraged to use all modern C++ features as long as they are supported by the C++ version we use.
Still be aware that in game loop code you should avoid allocations, so static buffers on the stack can be preferable.

Examples:
- Use `nullptr` instead of `0` or `NULL`.
- Use `std::fill` to initialize arrays when possible instead of using `mem_zero` or loops.

### Use `true` for success

Do not use `int` as return type for methods that can either succeed or fail.
Use `bool` instead. And `true` means success and `false` means failure.

See https://github.com/ddnet/ddnet/issues/6436

### Filenames

Names of files and folders should be all lowercase and words should be separated with underscores.

❌

```cpp
src/game/FooBar.cpp
```

✅

```cpp
src/game/foo_bar.cpp
```

## Code documentation

Code documentation is required for all public declarations of functions, classes etc. in the `base` folder.
For other code, documentation is recommended for functions, classes etc. intended for reuse or when it improves clarity.

We use [doxygen](https://www.doxygen.nl/) to generate code documentation.
The documentation is updated regularly and available at https://codedoc.ddnet.org/.

We use [Javadoc style block comments](https://www.doxygen.nl/manual/docblocks.html) and prefix [doxygen commands](https://www.doxygen.nl/manual/commands.html) with `@`, not with `\`.

## Commit messages

Describe the change your contribution is making for the player/user instead of talking about what you did in a technical sense. Your PR messages will ideally be in a format that can directly be used in the [change log](https://ddnet.org/downloads/).
