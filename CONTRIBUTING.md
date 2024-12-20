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
    + Existing ranks should not be made impossible.
    + Existing maps should not break.
    + New gameplay should not make runs easier on already completed maps.

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

```C++
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

```C++
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

### Teeworlds interpretation of the hungarian notation

DDNet inherited the hungarian notation like prefixes from [Teeworlds](https://www.teeworlds.com/?page=docs&wiki=nomenclature)

`m_`

Class member

`g_`

Global variable

`s_`

Static variable

`p`

Pointer

`a`

Fixed array

Combine them appropriately.
Class Prefixes

`C`

Class, CMyClass, This goes for structures as well.

`I`

Interface, IMyClass

Only use those prefixes. The ddnet code base does **NOT** follow the whole hungarian notation strictly.
Do **NOT** use `c` for constants or `b` for booleans or `i` for integers.

Examples:

```C++
class CFoo
{
    int m_Foo = 0;
    const char *m_pText = "";

    void Func(int Argument, int *pPointer)
    {
        int LocalVariable = 0;
    };
};
```

### The usage of `goto` is not encouraged

Do not use the `goto` keyword in new code, there are better control flow constructs in C++.

### Assignments in if statements should be avoided

Do not set variables in if statements.

❌

```C++
int Foo;
if((Foo = 2)) { .. }
```

✅

```C++
int Foo = 2;
if(Foo) { .. }
```

Unless the alternative code is more complex and harder to read.

### Using integers in boolean contexts should be avoided

❌

```C++
int Foo = 0;
if(!Foo) { .. }
```

✅

```C++
int Foo = 0;
if(Foo != 0) { .. }
```

### Methods with default arguments should be avoided

Default arguments tend to break quickly, if you have multiple you have to specify each even if you only want to change the last one.

### Method overloading should be avoided

Try finding descriptive names instead.

### Getters should not have a Get prefix

While the code base already has a lot of methods that start with a ``Get`` prefix. If new getters are added they should not contain a prefix.

❌

```C++
int GetMyVariable() { return m_MyVariable; }
```

✅

```C++
int MyVariable() { return m_MyVariable; }
```

### Class member variables should be initialized where they are declared

Instead of doing this ❌:

```C++
class CFoo
{
    int m_Foo;
};
```

Do this instead if possible ✅:

```C++
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

```C++
src/game/FooBar.cpp
```

✅

```C++
src/game/foo_bar.cpp
```

## Commit messages

Describe the change your contribution is making for the player/user instead of talking about what you did in a technical sense. Your PR messages will ideally be in a format that can directly be used in the [change log](https://ddnet.org/downloads/).
