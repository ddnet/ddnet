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

Such fix commits should ideally be squashed into one big commit using ``git commit --amend`` or ``git rebase -i``.

A lot of the style offenses can be fixed automatically by running the fix script `./scripts/fix_style.py`

We use clang-format 10. If your package manager no longer provides this version, you can download it from https://pypi.org/project/clang-format/10.0.1.1/.

### Upper camel case for variables, methods, class names

With the exception of base/system.{h,cpp}

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

More examples can be found [here](https://github.com/ddnet/ddnet/pull/8288#issuecomment-2094097306)

### Our interpretation of the hungarian notation

DDNet inherited the hungarian notation like prefixes from [Teeworlds](https://www.teeworlds.com/?page=docs&wiki=nomenclature)

Only use the prefixes listed below. The ddnet code base does **NOT** follow the whole hungarian notation strictly.

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

Combine these appropriately

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

### Getters should not have a Get prefix

While the code base already has a lot of methods that start with a ``Get`` prefix. If new getters are added they should not contain a prefix.

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

### Modern C++ should be used instead of old C styled code

DDNet balances in being portable (easy to compile on all common distributions) and using modern features.
So you are encouraged to use all modern C++ features as long as they are supported by the C++ version we use.
Still be aware that in game loop code you should avoid allocations, so static buffers on the stack can be preferable.

Examples:
- Use `nullptr` instead of `0` or `NULL`.

### Use `true` for success

Do not use `int` as return type for methods that can either succeed or fail.
Use `bool` instead. And `true` means success and `false` means failure.

See https://github.com/ddnet/ddnet/issues/6436

### Filenames

Code file names should be all lowercase and words should be separated with underscores.

❌

```cpp
src/game/FooBar.cpp
```

✅

```cpp
src/game/foo_bar.cpp
```

## Commit messages

Describe the change your contribution is making for the player/user instead of talking about what you did in a technical sense. Your PR messages will ideally be in a format that can directly be used in the [change log](https://ddnet.org/downloads/).
