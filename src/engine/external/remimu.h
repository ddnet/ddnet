#ifndef INCLUDE_REMIMU
#define INCLUDE_REMIMU

// NOTE: Stolen from https://github.com/wareya/Remimu/blob/main/my_regex.h

/************
 
    REMIMU: SINGLE HEADER C/C++ REGEX LIBRARY

    Compatible with C99 and C++11 and later standards. Uses backtracking and relatively standard regex syntax.

    #include "reregex.h"

FUNCTIONS

    // Returns 0 on success, or -1 on invalid or unsupported regex, or -2 on not enough tokens given to parse regex.
    static inline int regex_parse(
        const char * pattern,       // Regex pattern to parse.
        RegexToken * tokens,        // Output buffer of token_count regex tokens.
        int16_t * token_count,      // Maximum allowed number of tokens to write
        int32_t flags               // Optional bitflags.
    )
    
    // Returns match length, or -1 on no match, or -2 on out of memory, or -3 if the regex is invalid.
    static inline int64_t regex_match(
        const RegexToken * tokens,  // Parsed regex to match against text.
        const char * text,          // Text to match against tokens.
        size_t start_i,             // index value to match at.
        uint16_t cap_slots,         // Number of allowed capture info output slots.
        int64_t * cap_pos,          // Capture position info output buffer.
        int64_t * cap_span          // Capture length info output buffer.
    ) 
    
    static inline void print_regex_tokens(
        RegexToken * tokens     // Regex tokens to spew to stdout, for debugging.
    )

PERFORMANCE

    On simple cases, Remimu's match speed is similar to PCRE2. Regex parsing/compilation is also much faster (around 4x to 10x), so single-shot regexes are often faster than PCRE2.

    HOWEVER: Remimu is a pure backtracking engine, and has `O(2^x)` complexity on regexes with catastrophic backtracking. It can be much, much, MUCH slower than PCRE2. Beware!

    Remimu uses length-checked fixed memory buffers with no recursion, so memory usage is statically known.

FEATURES

    - Lowest-common-denominator common regex syntax
    - Based on backtracking (slow in the worst case, but fast in the best case)
    - 8-bit only, not unicode
    - Statically known memory usage (no heap allocation or recursion)
    - Groups with or without capture, and with or without quantifiers
    - Supported escapes:
    - - 2-digit hex: e.g. \x00, \xFF, or lowercase, or mixed case
    - - \r, \n, \t, \v, \f (whitespace characters)
    - - \d, \s, \w, \D, \S, \W (digit, space, and word character classes)
    - - \b, \B word boundary and non-word-boundary anchors (not fully supported in zero-size quantified groups, but even then, usually supported)
    - - Escaped literal characters: {}[]-()|^$*+?:./\
    - - - Escapes work in character classes, except for 'b'
    - Character classes, including disjoint ranges, proper handling of bare [ and trailing -, etc
    - - Dot (.) matches all characters, including newlines, unless REMIMU_FLAG_DOT_NO_NEWLINES is passed as a flag to regex_parse
    - - Dot (.) only matches at most one byte at a time, so matching \r\n requires two dots (and not using REMIMU_FLAG_DOT_NO_NEWLINES)
    - Anchors (^ and $)
    - - Same support caveats as \b, \B apply
    - Basic quantifiers (*, +, ?)
    - - Quantifiers are greedy by default.
    - Explicit quantifiers ({2}, {5}, {5,}, {5,7})
    - Alternation e.g. (asdf|foo)
    - Lazy quantifiers e.g. (asdf)*? or \w+?
    - Possessive greedy quantifiers e.g. (asdf)*+ or \w++
    - - NOTE: Capture groups for and inside of possessive groups return no capture information.
    - Atomic groups e.g. (?>(asdf))
    - - NOTE: Capture groups inside of atomic groups return no capture information.

NOT SUPPORTED

    - Strings with non-terminal null characters
    - Unicode, unicode character classes, etc
    - Exact POSIX regex semantics (posix-style greediness etc)
    - Backreferences
    - Lookbehind/Lookahead
    - Named groups
    - Most other weird flavor-specific regex stuff
    - Capture of or inside of possessive-quantified groups (still take up a capture slot, but no data is returned)

USAGE

    // minimal:
    
    RegexToken tokens[1024];
    int16_t token_count = 1024;
    int e = regex_parse("[0-9]+\\.[0-9]+", tokens, &token_count, 0);
    assert(!e);
    
    int64_t match_len = regex_match(tokens, "23.53) ", 0, 0, 0, 0);
    printf("########### return: %zd\n", match_len);
    
    // with captures:
    
    RegexToken tokens[256];
    int16_t token_count = sizeof(tokens)/sizeof(tokens[0]);
    int e = regex_parse("((a)|(b))++", tokens, &token_count, 0);
    assert(!e);
    
    int64_t cap_pos[5];
    int64_t cap_span[5];
    memset(cap_pos, 0xFF, sizeof(cap_pos));
    memset(cap_span, 0xFF, sizeof(cap_span));
    
    int64_t matchlen = regex_match(tokens, "aaaaaabbbabaqa", 0, 5, cap_pos, cap_span);
    printf("Match length: %zd\n", matchlen);
    for (int i = 0; i < 5; i++)
        printf("Capture %d: %zd plus %zd\n", i, cap_pos[i], cap_span[i]);
    
    // for debugging
    print_regex_tokens(tokens);

LICENSE

    Creative Commons Zero, public domain.

*/

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

static const int REMIMU_FLAG_DOT_NO_NEWLINES = 1;

static const uint8_t REMIMU_KIND_NORMAL      = 0;
static const uint8_t REMIMU_KIND_OPEN        = 1;
static const uint8_t REMIMU_KIND_NCOPEN      = 2;
static const uint8_t REMIMU_KIND_CLOSE       = 3;
static const uint8_t REMIMU_KIND_OR          = 4;
static const uint8_t REMIMU_KIND_CARET       = 5;
static const uint8_t REMIMU_KIND_DOLLAR      = 6;
static const uint8_t REMIMU_KIND_BOUND       = 7;
static const uint8_t REMIMU_KIND_NBOUND      = 8;
static const uint8_t REMIMU_KIND_END         = 9;

static const uint8_t REMIMU_MODE_POSSESSIVE  = 1;
static const uint8_t REMIMU_MODE_LAZY        = 2;
static const uint8_t REMIMU_MODE_INVERTED    = 128; // temporary; gets cleared later

typedef struct _RegexToken {
    uint8_t kind;
    uint8_t mode;
    uint16_t count_lo;
    uint16_t count_hi; // 0 means no limit
    uint16_t mask[16]; // for groups: mask 0 stores group-with-quantifier number (quantifiers are +, *, ?, {n}, {n,}, or {n,m})
    int16_t pair_offset; // from ( or ), offset in token list to matching paren. TODO: move into mask maybe
} RegexToken;

/// Returns a negative number on failure:
/// -1: Regex string is invalid or using unsupported features or too long.
/// -2: Provided buffer not long enough. Give up, or reallocate with more length and retry.
/// Returns 0 on success.
/// On call, token_count pointer must point to the number of tokens that can be written to the tokens buffer.
/// On successful return, the number of actually used tokens is written to token_count.
/// Sets token_count to zero if a regex is not created but no error happened (e.g. empty pattern).
/// Flags: Not yet used.
/// SAFETY: Pattern must be null-terminated.
/// SAFETY: tokens buffer must have at least the input token_count number of RegexToken objects. They are allowed to be uninitialized.
static inline int regex_parse(const char * pattern, RegexToken * tokens, int16_t * token_count, int32_t flags)
{
    if (token_count == 0)
        return -2;
    int64_t tokens_len = *token_count;
    uint64_t pattern_len = strlen(pattern);
    
    // 0: normal
    // 1: just saw a backslash
    int esc_state = 0;
    
    // 0: init
    // 1: normal
    // 2: in char class, initial state
    // 3: in char class, but possibly looking for a range marker
    // 4: in char class, but just saw a range marker
    // 5: immediately after quantifiable token
    // 6: immediately after quantifier
    
    const int STATE_NORMAL    = 1;
    const int STATE_QUANT     = 2;
    const int STATE_MODE      = 3;
    const int STATE_CC_INIT   = 4;
    const int STATE_CC_NORMAL = 5;
    const int STATE_CC_RANGE  = 6;
    int state = STATE_NORMAL;
    
    int char_class_mem = -1;
    
    RegexToken token;
    #define _REGEX_CLEAR_TOKEN(TOKEN) { memset(&(TOKEN), 0, sizeof(RegexToken)); token.count_lo = 1; token.count_hi = 2; }
    _REGEX_CLEAR_TOKEN(token);
    
    #define _REGEX_DO_INVERT() { \
        for (int n = 0; n < 16; n++) \
            token.mask[n] = ~token.mask[n]; \
        token.mode &= ~REMIMU_MODE_INVERTED; \
    }
    
    int16_t k = 0;
    
    #define _REGEX_PUSH_TOKEN() { \
        if (k == 0 || tokens[k-1].kind != token.kind || (token.kind != REMIMU_KIND_BOUND && token.kind != REMIMU_KIND_NBOUND)) \
        { \
            if (token.mode & REMIMU_MODE_INVERTED) _REGEX_DO_INVERT() \
            if (k >= tokens_len) \
            { \
                puts("buffer overflow"); \
                return -2; \
            } \
            tokens[k++] = token; \
            _REGEX_CLEAR_TOKEN(token); \
        } \
    }
    
    #define _REGEX_SET_MASK(byte) { token.mask[((uint8_t)(byte))>>4] |= 1 << ((uint8_t)(byte) & 0xF); }
    #define _REGEX_SET_MASK_ALL() { \
        for (int n = 0; n < 16; n++) \
            token.mask[n] = 0xFFFF; \
    }
    
    // start with an invisible group specifier
    // (this allows the matcher to not need to have a special root-level alternation operator case)
    token.kind = REMIMU_KIND_OPEN;
    token.count_lo = 0;
    token.count_hi = 0;

    int paren_count = 0;
    
    for (uint64_t i = 0; i < pattern_len; i++)
    {
        char c = pattern[i];
        if (state == STATE_QUANT)
        {
            state = STATE_MODE;
            if (c == '?')
            {
                token.count_lo = 0;
                token.count_hi = 2; // first non-allowed amount
                continue;
            }
            else if (c == '+')
            {
                token.count_lo = 1;
                token.count_hi = 0; // unlimited
                continue;
            }
            else if (c == '*')
            {
                token.count_lo = 0;
                token.count_hi = 0; // unlimited
                continue;
            }
            else if (c == '{')
            {
                if (pattern[i+1] == 0 || pattern[i+1] < '0' || pattern[i+1] > '9')
                    state = STATE_NORMAL;
                else
                {
                    i += 1;
                    uint32_t val = 0;
                    while (pattern[i] >= '0' && pattern[i] <= '9')
                    {
                        val *= 10;
                        val += (uint32_t)(pattern[i] - '0');
                        if (val > 0xFFFF)
                        {
                            puts("quantifier range too long");
                            return -1; // unsupported length
                        }
                        i += 1;
                    }
                    token.count_lo = val;
                    token.count_hi = val + 1;
                    if (pattern[i] == ',')
                    {
                        token.count_hi = 0; // unlimited
                        i += 1;
                        
                        if (pattern[i] >= '0' && pattern[i] <= '9')
                        {
                            uint32_t val2 = 0;
                            while (pattern[i] >= '0' && pattern[i] <= '9')
                            {
                                val2 *= 10;
                                val2 += (uint32_t)(pattern[i] - '0');
                                if (val2 > 0xFFFF)
                                {
                                    puts("quantifier range too long");
                                    return -1; // unsupported length
                                }
                                i += 1;
                            }
                            if (val2 < val)
                            {
                                puts("quantifier range is backwards");
                                return -1; // unsupported length
                            }
                            token.count_hi = val2 + 1;
                        }
                    }
                    
                    if (pattern[i] == '}')
                    {
                        // quantifier range parsed successfully
                        continue;
                    }
                    else
                    {
                        puts("quantifier range syntax broken (no terminator)");
                        return -1;
                    }
                }
            }
        }
        
        if (state == STATE_MODE)
        {
            state = STATE_NORMAL;
            if (c == '?')
            {
                token.mode |= REMIMU_MODE_LAZY;
                continue;
            }
            else if (c == '+')
            {
                token.mode |= REMIMU_MODE_POSSESSIVE;
                continue;
            }
        }
        
        if (state == STATE_NORMAL)
        {
            if (esc_state == 1)
            {
                esc_state = 0;
                if (c == 'n')
                    _REGEX_SET_MASK('\n')
                else if (c == 'r')
                    _REGEX_SET_MASK('\r')
                else if (c == 't')
                    _REGEX_SET_MASK('\t')
                else if (c == 'v')
                    _REGEX_SET_MASK('\v')
                else if (c == 'f')
                    _REGEX_SET_MASK('\f')
                else if (c == 'x')
                {
                    if (pattern[i+1] == 0 || pattern[i+2] == 0)
                        return -1; // too-short hex pattern
                    uint8_t n0 = pattern[i+1];
                    uint8_t n1 = pattern[i+1];
                    if (n0 < '0' || n0 > 'f' || n1 < '0' || n1 > 'f' ||
                        (n0 > '9' && n0 < 'A') || (n1 > '9' && n1 < 'A'))
                        return -1; // invalid hex digit
                    if (n0 > 'F') n0 -= 0x20;
                    if (n1 > 'F') n1 -= 0x20;
                    if (n0 >= 'A') n0 -= 'A' - 10;
                    if (n1 >= 'A') n1 -= 'A' - 10;
                    n0 -= '0';
                    n1 -= '0';
                    _REGEX_SET_MASK((n1 << 4) | n0)
                    i += 2;
                }
                else if (c == '{' || c == '}' ||
                         c == '[' || c == ']' || c == '-' ||
                         c == '(' || c == ')' ||
                         c == '|' || c == '^' || c == '$' ||
                         c == '*' || c == '+' || c == '?' || c == ':' ||
                         c == '.' || c == '/' || c == '\\')
                {
                    _REGEX_SET_MASK(c)
                    state = STATE_QUANT;
                }
                else if (c == 'd' || c == 's' || c == 'w' ||
                         c == 'D' || c == 'S' || c == 'W')
                {
                    uint8_t is_upper = c <= 'Z';
                    
                    uint16_t m[16];
                    memset(m, 0, sizeof(m));
                    
                    if (is_upper)
                        c += 0x20;
                    if (c == 'd' || c == 'w')
                        m[3] |= 0x03FF; // 0~7
                    if (c == 's')
                    {
                        m[0] |= 0x3E00; // \t-\r (includes \n, \v, and \f in the middle. 5 enabled bits.)
                        m[2] |= 1; // ' '
                    }
                    if (c == 'w')
                    {
                        m[4] |= 0xFFFE; // A-O
                        m[5] |= 0x87FF; // P-Z_
                        m[6] |= 0xFFFE; // a-o
                        m[7] |= 0x07FF; // p-z
                    }
                    
                    for (int j = 0; j < 16; j++)
                        token.mask[j] |= is_upper ? ~m[j] : m[j];
                    
                    token.kind = REMIMU_KIND_NORMAL;
                    state = STATE_QUANT;
                }
                else if (c == 'b')
                {
                    token.kind = REMIMU_KIND_BOUND;
                    state = STATE_NORMAL;
                }
                else if (c == 'B')
                {
                    token.kind = REMIMU_KIND_NBOUND;
                    state = STATE_NORMAL;
                }
                else
                {
                    puts("unsupported escape sequence");
                    return -1; // unknown/unsupported escape sequence
                }
            }
            else
            {
                _REGEX_PUSH_TOKEN()
                if (c == '\\')
                {
                    esc_state = 1;
                }
                else if (c == '[')
                {
                    state = STATE_CC_INIT;
                    char_class_mem = -1;
                    token.kind = REMIMU_KIND_NORMAL;
                    if (pattern[i + 1] == '^')
                    {
                        token.mode |= REMIMU_MODE_INVERTED;
                        i += 1;
                    }
                }
                else if (c == '(')
                {
                    paren_count += 1;
                    state = STATE_NORMAL;
                    token.kind = REMIMU_KIND_OPEN;
                    token.count_lo = 0;
                    token.count_hi = 1;
                    if (pattern[i + 1] == '?' && pattern[i + 2] == ':')
                    {
                        token.kind = REMIMU_KIND_NCOPEN;
                        i += 2;
                    }
                    else if (pattern[i + 1] == '?' && pattern[i + 2] == '>')
                    {
                        token.kind = REMIMU_KIND_NCOPEN;
                        _REGEX_PUSH_TOKEN()
                        
                        state = STATE_NORMAL;
                        token.kind = REMIMU_KIND_NCOPEN;
                        token.mode = REMIMU_MODE_POSSESSIVE;
                        token.count_lo = 1;
                        token.count_hi = 2;
                        
                        i += 2;
                    }
                }
                else if (c == ')')
                {
                    paren_count -= 1;
                    if (paren_count < 0 || k == 0)
                        return -1; // unbalanced parens
                    token.kind = REMIMU_KIND_CLOSE;
                    state = STATE_QUANT;
                    
                    int balance = 0;
                    ptrdiff_t found = -1;
                    for (ptrdiff_t l = k - 1; l >= 0; l--)
                    {
                        if (tokens[l].kind == REMIMU_KIND_NCOPEN || tokens[l].kind == REMIMU_KIND_OPEN)
                        {
                            if (balance == 0)
                            {
                                found = l;
                                break;
                            }
                            else
                                balance -= 1;
                        }
                        else if (tokens[l].kind == REMIMU_KIND_CLOSE)
                            balance += 1;
                    }
                    if (found == -1)
                        return -1; // unbalanced parens
                    ptrdiff_t diff = k - found;
                    if (diff > 32767)
                        return -1; // too long
                    token.pair_offset = -diff;
                    tokens[found].pair_offset = diff;
                    // phantom group for atomic group emulation
                    if (tokens[found].mode == REMIMU_MODE_POSSESSIVE)
                    {
                        _REGEX_PUSH_TOKEN()
                        token.kind = REMIMU_KIND_CLOSE;
                        token.mode = REMIMU_MODE_POSSESSIVE;
                        token.pair_offset = -diff - 2;
                        tokens[found - 1].pair_offset = diff + 2;
                    }
                }
                else if (c == '?' || c == '+' || c == '*' || c == '{')
                {
                    puts("quantifier in non-quantifier context");
                    return -1; // quantifier in non-quantifier context
                }
                else if (c == '.')
                {
                    //puts("setting ALL of mask...");
                    _REGEX_SET_MASK_ALL();
                    if (flags & REMIMU_FLAG_DOT_NO_NEWLINES)
                    {
                        token.mask[1] ^= 0x04; // \n
                        token.mask[1] ^= 0x20; // \r
                    }
                    state = STATE_QUANT;
                }
                else if (c == '^')
                {
                    token.kind = REMIMU_KIND_CARET;
                    state = STATE_NORMAL;
                }
                else if (c == '$')
                {
                    token.kind = REMIMU_KIND_DOLLAR;
                    state = STATE_NORMAL;
                }
                else if (c == '|')
                {
                    token.kind = REMIMU_KIND_OR;
                    state = STATE_NORMAL;
                }
                else
                {
                    _REGEX_SET_MASK(c);
                    state = STATE_QUANT;
                }
            }
        }
        else if (state == STATE_CC_INIT || state == STATE_CC_NORMAL || state == STATE_CC_RANGE)
        {
            if (c == '\\' && esc_state == 0)
            {
                esc_state = 1;
                continue;
            }
            uint8_t esc_c = 0;
            if (esc_state == 1)
            {
                esc_state = 0;
                if (c == 'n')
                    esc_c = '\n';
                else if (c == 'r')
                    esc_c = '\r';
                else if (c == 't')
                    esc_c = '\t';
                else if (c == 'v')
                    esc_c = '\v';
                else if (c == 'f')
                    esc_c = '\f';
                else if (c == 'x')
                {
                    if (pattern[i+1] == 0 || pattern[i+2] == 0)
                        return -1; // too-short hex pattern
                    uint8_t n0 = pattern[i+1];
                    uint8_t n1 = pattern[i+1];
                    if (n0 < '0' || n0 > 'f' || n1 < '0' || n1 > 'f' ||
                        (n0 > '9' && n0 < 'A') || (n1 > '9' && n1 < 'A'))
                        return -1; // invalid hex digit
                    if (n0 > 'F') n0 -= 0x20;
                    if (n1 > 'F') n1 -= 0x20;
                    if (n0 >= 'A') n0 -= 'A' - 10;
                    if (n1 >= 'A') n1 -= 'A' - 10;
                    n0 -= '0';
                    n1 -= '0';
                    esc_c = (n1 << 4) | n0;
                    i += 2;
                }
                else if (c == '{' || c == '}' ||
                         c == '[' || c == ']' || c == '-' ||
                         c == '(' || c == ')' ||
                         c == '|' || c == '^' || c == '$' ||
                         c == '*' || c == '+' || c == '?' || c == ':' ||
                         c == '.' || c == '/' || c == '\\')
                {
                    esc_c = c;
                }
                else if (c == 'd' || c == 's' || c == 'w' ||
                         c == 'D' || c == 'S' || c == 'W')
                {
                    if (state == STATE_CC_RANGE)
                    {
                        puts("tried to use a shorthand as part of a range");
                        return -1; // range shorthands can't be part of a range
                    }
                    uint8_t is_upper = c <= 'Z';
                    
                    uint16_t m[16];
                    memset(m, 0, sizeof(m));
                    
                    if (is_upper)
                        c += 0x20;
                    if (c == 'd' || c == 'w')
                        m[3] |= 0x03FF; // 0~7
                    if (c == 's')
                    {
                        m[0] |= 0x3E00; // \t-\r (includes \n, \v, and \f in the middle. 5 enabled bits.)
                        m[2] |= 1; // ' '
                    }
                    if (c == 'w')
                    {
                        m[4] |= 0xFFFE; // A-O
                        m[5] |= 0x87FF; // P-Z_
                        m[6] |= 0xFFFE; // a-o
                        m[7] |= 0x07FF; // p-z
                    }
                    
                    for (int j = 0; j < 16; j++)
                        token.mask[j] |= is_upper ? ~m[j] : m[j];
                    
                    char_class_mem = -1; // range shorthands can't be part of a range
                    continue;
                }
                else
                {
                    printf("unknown/unsupported escape sequence in character class (\\%c)\n", c);
                    return -1; // unknown/unsupported escape sequence
                }
            }
            if (state == STATE_CC_INIT)
            {
                char_class_mem = c;
                _REGEX_SET_MASK(c);
                state = STATE_CC_NORMAL;
            }
            else if (state == STATE_CC_NORMAL)
            {
                if (c == ']' && esc_c == 0)
                {
                    char_class_mem = -1;
                    state = STATE_QUANT;
                    continue;
                }
                else if (c == '-' && esc_c == 0 && char_class_mem >= 0)
                {
                    state = STATE_CC_RANGE;
                    continue;
                }
                else
                {
                    char_class_mem = c;
                    _REGEX_SET_MASK(c);
                    state = STATE_CC_NORMAL;
                }
            }
            else if (state == STATE_CC_RANGE)
            {
                if (c == ']' && esc_c == 0)
                {
                    char_class_mem = -1;
                    _REGEX_SET_MASK('-');
                    state = STATE_QUANT;
                    continue;
                }
                else
                {
                    if (char_class_mem == -1)
                    {
                        puts("character class range is broken");
                        return -1; // probably tried to use a character class shorthand as part of a range
                    }
                    if ((uint8_t)c < char_class_mem)
                    {
                        puts("character class range is misordered");
                        return -1; // range is in wrong order
                    }
                    //printf("enabling char class from %d to %d...\n", char_class_mem, c);
                    for (uint8_t j = c; j > char_class_mem; j--)
                        _REGEX_SET_MASK(j);
                    state = STATE_CC_NORMAL;
                    char_class_mem = -1;
                }
            }
        }
        else
            assert(0);
    }
    if (paren_count > 0)
    {
        puts("(paren_count > 0)");
        return -1; // unbalanced parens
    }
    if (esc_state != 0)
    {
        puts("(esc_state != 0)");
        return -1; // open escape sequence
    }
    if (state >= STATE_CC_INIT)
    {
        puts("(state >= STATE_CC_INIT)");
        return -1; // open character class
    }
    
    _REGEX_PUSH_TOKEN()
    
    // add invisible non-capturing group specifier
    token.kind = REMIMU_KIND_CLOSE;
    token.count_lo = 1;
    token.count_hi = 2;
    _REGEX_PUSH_TOKEN();
    
    // add end token (tells matcher that it's done)
    token.kind = REMIMU_KIND_END;
    _REGEX_PUSH_TOKEN();
    
    tokens[0].pair_offset = k - 2;
    tokens[k-2].pair_offset = -(k - 2);
    
    *token_count = k;
    
    // copy quantifiers from )s to (s (so (s know whether they're optional)
    // also take the opportunity to smuggle "quantified group index" into the mask field for the )
    uint64_t n = 0;
    for (int16_t k2 = 0; k2 < k; k2++)
    {
        if (tokens[k2].kind == REMIMU_KIND_CLOSE)
        {
            tokens[k2].mask[0] = n++;
            
            int16_t k3 = k2 + tokens[k2].pair_offset;
            tokens[k3].count_lo = tokens[k2].count_lo;
            tokens[k3].count_hi = tokens[k2].count_hi;
            tokens[k3].mask[0] = n++;
            tokens[k3].mode = tokens[k2].mode;
            
            //if (n > 65535)
            if (n > 1024)
                return -1; // too many quantified groups
        }
        else if (tokens[k2].kind == REMIMU_KIND_OR || tokens[k2].kind == REMIMU_KIND_OPEN || tokens[k2].kind == REMIMU_KIND_NCOPEN)
        {
            // find next | or ) and how far away it is. store in token
            int balance = 0;
            ptrdiff_t found = -1;
            for (ptrdiff_t l = k2 + 1; l < tokens_len; l++)
            {
                if (tokens[l].kind == REMIMU_KIND_OR && balance == 0)
                {
                    found = l;
                    break;
                }
                else if (tokens[l].kind == REMIMU_KIND_CLOSE)
                {
                    if (balance == 0)
                    {
                        found = l;
                        break;
                    }
                    else
                        balance -= 1;
                }
                else if (tokens[l].kind == REMIMU_KIND_NCOPEN || tokens[l].kind == REMIMU_KIND_OPEN)
                    balance += 1;
            }
            if (found == -1)
            {
                puts("unbalanced parens...");
                return -1; // unbalanced parens
            }
            ptrdiff_t diff = found - k2;
            if (diff > 32767)
            {
                puts("too long...");
                return -1; // too long
            }
            
            if (tokens[k2].kind == REMIMU_KIND_OR)
                tokens[k2].pair_offset = diff;
            else
                tokens[k2].mask[15] = diff;
        }
    }
    
    #undef _REGEX_PUSH_TOKEN
    #undef _REGEX_SET_MASK
    #undef _REGEX_CLEAR_TOKEN
    
    return 0;
}

typedef struct _RegexMatcherState {
    uint32_t k;
    uint32_t group_state; // quantified group temp state (e.g. number of repetitions)
    uint32_t prev; // for )s, stack index of corresponding previous quantified state
#ifdef REGEX_STACK_SMOL
    uint32_t i;
    uint32_t range_min;
    uint32_t range_max;
#else
    uint64_t i;
    uint64_t range_min;
    uint64_t range_max;
#endif
} RegexMatcherState;

// NOTE: undef'd later
#define _REGEX_CHECK_MASK(K, byte) (!!(tokens[K].mask[((uint8_t)byte)>>4] & (1 << ((uint8_t)byte & 0xF))))

// Returns match length if text starts with a regex match.
// Returns -1 if the text doesn't start with a regex match.
// Returns -2 if the matcher ran out of memory or the regex is too complex.
// Returns -3 if the regex is somehow invalid.
// The first cap_slots capture positions and spans (lengths) will be written to cap_pos and cap_span. If zero, will not be written to.
// SAFETY: The text variable must be null-terminated, and start_i must be the index of a character within the string or its null terminator.
// SAFETY: Tokens array must be terminated by a REMIMU_KIND_END token (done by default by regex_parse).
// SAFETY: Partial capture data may be written even if the match fails.
static inline int64_t regex_match(const RegexToken * tokens, const char * text, size_t start_i, uint16_t cap_slots, int64_t * cap_pos, int64_t * cap_span)
{
    (void)text;
    
#ifdef REGEX_VERBOSE
    const uint8_t verbose = 1;
#else
    const uint8_t verbose = 0;
#endif
    
#define IF_VERBOSE(X) { if (verbose) { X } }
    
#ifdef REGEX_STACK_SMOL
    const uint16_t stack_size_max = 256;
#else
    const uint16_t stack_size_max = 1024;
#endif
    const uint16_t aux_stats_size = 1024;
    if (cap_slots > aux_stats_size)
        cap_slots = aux_stats_size;
    
    // quantified group state
    uint8_t q_group_accepts_zero[aux_stats_size];
    uint32_t q_group_state[aux_stats_size]; // number of repetitions
    uint32_t q_group_stack[aux_stats_size]; // location of most recent corresponding ) on stack. 0 means nowhere
    
    uint16_t q_group_cap_index[aux_stats_size];
    memset(q_group_cap_index, 0xFF, sizeof(q_group_cap_index));
    
    uint64_t tokens_len = 0;
    uint32_t k = 0;
    uint16_t caps = 0;
    
    while (tokens[k].kind != REMIMU_KIND_END)
    {
        if (tokens[k].kind == REMIMU_KIND_OPEN && caps < cap_slots)
        {
            q_group_cap_index[tokens[k].mask[0]] = caps;
            q_group_cap_index[tokens[k + tokens[k].pair_offset].mask[0]] = caps;
            cap_pos[caps] = -1;
            cap_span[caps] = -1;
            caps += 1;
        }
        k += 1;
        if (tokens[k].kind == REMIMU_KIND_CLOSE || tokens[k].kind == REMIMU_KIND_OPEN || tokens[k].kind == REMIMU_KIND_NCOPEN)
        {
            if (tokens[k].mask[0] >= aux_stats_size)
            {
                puts("too many qualified groups. returning");
                return -2; // OOM: too many quantified groups
            }
            
            q_group_state[tokens[k].mask[0]] = 0;
            q_group_stack[tokens[k].mask[0]] = 0;
            q_group_accepts_zero[tokens[k].mask[0]] = 0;
        }
    }
    
    tokens_len = k;
    
    RegexMatcherState rewind_stack[stack_size_max];
    uint16_t stack_n = 0;
    
    uint64_t i = start_i;
    
    uint64_t range_min = 0;
    uint64_t range_max = 0;
    uint8_t just_rewinded = 0;
    
    #define _P_TEXT_HIGHLIGHTED() { \
        IF_VERBOSE(printf("\033[91m"); \
        for (uint64_t q = 0; q < i; q++) printf("%c", text[q]); \
        printf("\033[0m"); \
        for (uint64_t q = i; text[q] != 0; q++) printf("%c", text[q]); \
        printf("\n");) \
    }
    
    #define _REWIND_DO_SAVE_RAW(K, ISDUMMY) { \
        if (stack_n >= stack_size_max) \
        { \
            puts("out of backtracking room. returning"); \
            return -2; \
        } \
        RegexMatcherState s; \
        memset(&s, 0, sizeof(RegexMatcherState)); \
        s.i = i; \
        s.k = (K); \
        s.range_min = range_min; \
        s.range_max = range_max; \
        s.prev = 0; \
        if (ISDUMMY) s.prev = 0xFAC7; \
        else if (tokens[s.k].kind == REMIMU_KIND_CLOSE) \
        { \
            s.group_state = q_group_state[tokens[s.k].mask[0]]; \
            s.prev = q_group_stack[tokens[s.k].mask[0]]; \
            q_group_stack[tokens[s.k].mask[0]] = stack_n; \
        } \
        rewind_stack[stack_n++] = s; \
        _P_TEXT_HIGHLIGHTED() \
        IF_VERBOSE(printf("-- saving rewind state k %u i %zd rmin %zu rmax %zd (line %d) (depth %d prev %d)\n", s.k, i, range_min, range_max, __LINE__, stack_n, s.prev);) \
    }
    #define _REWIND_DO_SAVE_DUMMY(K) _REWIND_DO_SAVE_RAW(K, 1)
    #define _REWIND_DO_SAVE(K) _REWIND_DO_SAVE_RAW(K, 0)
    
    #define _REWIND_OR_ABORT() { \
        if (stack_n == 0) \
            return -1; \
        stack_n -= 1; \
        while (stack_n > 0 && rewind_stack[stack_n].prev == 0xFAC7) stack_n -= 1; \
        just_rewinded = 1; \
        range_min = rewind_stack[stack_n].range_min; \
        range_max = rewind_stack[stack_n].range_max; \
        assert(rewind_stack[stack_n].i <= i); \
        i = rewind_stack[stack_n].i; \
        k = rewind_stack[stack_n].k; \
        if (tokens[k].kind == REMIMU_KIND_CLOSE) \
        { \
            q_group_state[tokens[k].mask[0]] = rewind_stack[stack_n].group_state; \
            q_group_stack[tokens[k].mask[0]] = rewind_stack[stack_n].prev; \
        } \
        _P_TEXT_HIGHLIGHTED() \
        IF_VERBOSE(printf("-- rewound to k %u i %zd rmin %zu rmax %zd (kind %d prev %d)\n", k, i, range_min, range_max, tokens[k].kind, rewind_stack[stack_n].prev);) \
        k -= 1; \
    }
    // the -= 1 is because of the k++ in the for loop
    
    // used in boundary anchor checker
    uint64_t w_mask[16];
    memset(w_mask, 0, sizeof(w_mask));
    w_mask[3] = 0x03FF;
    w_mask[4] = 0xFFFE;
    w_mask[5] = 0x87FF;
    w_mask[6] = 0xFFFE;
    w_mask[7] = 0x07FF;
    #define _REGEX_CHECK_IS_W(byte) (!!(w_mask[((uint8_t)byte)>>4] & (1 << ((uint8_t)byte & 0xF))))
    
    size_t limit = 10000;
    for (k = 0; k < tokens_len; k++)
    {
        //limit--;
        if (limit == 0)
        {
            puts("iteration limit exceeded. returning");
            return -2;
        }
        IF_VERBOSE(printf("k: %u\ti: %zu\tl: %zu\tstack_n: %d\n", k, i, limit, stack_n);)
        _P_TEXT_HIGHLIGHTED();
        if (tokens[k].kind == REMIMU_KIND_CARET)
        {
            if (i != 0)
                _REWIND_OR_ABORT()
            continue;
        }
        else if (tokens[k].kind == REMIMU_KIND_DOLLAR)
        {
            if (text[i] != 0)
                _REWIND_OR_ABORT()
            continue;
        }
        else if (tokens[k].kind == REMIMU_KIND_BOUND)
        {
            if (i == 0 && !_REGEX_CHECK_IS_W(text[i]))
                _REWIND_OR_ABORT()
            else if (i != 0 && text[i] == 0 && !_REGEX_CHECK_IS_W(text[i-1]))
                _REWIND_OR_ABORT()
            else if (i != 0 && text[i] != 0 && _REGEX_CHECK_IS_W(text[i-1]) == _REGEX_CHECK_IS_W(text[i]))
                _REWIND_OR_ABORT()
        }
        else if (tokens[k].kind == REMIMU_KIND_NBOUND)
        {
            if (i == 0 && _REGEX_CHECK_IS_W(text[i]))
                _REWIND_OR_ABORT()
            else if (i != 0 && text[i] == 0 && _REGEX_CHECK_IS_W(text[i-1]))
                _REWIND_OR_ABORT()
            else if (i != 0 && text[i] != 0 && _REGEX_CHECK_IS_W(text[i-1]) != _REGEX_CHECK_IS_W(text[i]))
                _REWIND_OR_ABORT()
        }
        else
        {
            // deliberately unmatchable token (e.g. a{0}, a{0,0})
            if (tokens[k].count_hi == 1)
            {
                if (tokens[k].kind == REMIMU_KIND_OPEN || tokens[k].kind == REMIMU_KIND_NCOPEN)
                    k += tokens[k].pair_offset;
                else
                    k += 1;
                continue;
            }
            
            if (tokens[k].kind == REMIMU_KIND_OPEN || tokens[k].kind == REMIMU_KIND_NCOPEN)
            {
                if (!just_rewinded)
                {
                    IF_VERBOSE(printf("hit OPEN. i is %zd, depth is %d\n", i, stack_n);)
                    // need this to be able to detect and reject zero-size matches
                    //q_group_state[tokens[k].mask[0]] = i;
                    
                    // if we're lazy and the min length is 0, we need to try the non-group case first
                    if ((tokens[k].mode & REMIMU_MODE_LAZY) && (tokens[k].count_lo == 0 || q_group_accepts_zero[tokens[k + tokens[k].pair_offset].mask[0]]))
                    {
                        IF_VERBOSE(puts("trying non-group case first.....");)
                        range_min = 0;
                        range_max = 0;
                        _REWIND_DO_SAVE(k)
                        k += tokens[k].pair_offset; // automatic += 1 will put us past the matching )
                    }
                    else
                    {
                        range_min = 1;
                        range_max = 0;
                        _REWIND_DO_SAVE(k)
                    }
                }
                else
                {
                    IF_VERBOSE(printf("rewinded into OPEN. i is %zd, depth is %d\n", i, stack_n);)
                    just_rewinded = 0;
                    
                    uint64_t orig_k = k;
                    
                    IF_VERBOSE(printf("--- trying to try another alternation, start k is %d, rmin is %zu\n", k, range_min);)
                    
                    if (range_min != 0)
                    {
                        IF_VERBOSE(puts("rangemin is not zero. checking...");)
                        k += range_min;
                        IF_VERBOSE(printf("start kind: %d\n", tokens[k].kind);)
                        IF_VERBOSE(printf("before start kind: %d\n", tokens[k-1].kind);)
                        if (tokens[k-1].kind == REMIMU_KIND_OR)
                            k += tokens[k-1].pair_offset - 1;
                        else if (tokens[k-1].kind == REMIMU_KIND_OPEN || tokens[k-1].kind == REMIMU_KIND_NCOPEN)
                            k += tokens[k-1].mask[15] - 1;
                        
                        IF_VERBOSE(printf("kamakama %d %d\n", k, tokens[k].kind);)
                        
                        if (tokens[k].kind == REMIMU_KIND_END) // unbalanced parens
                            return -3;
                        
                        IF_VERBOSE(printf("---?!?!   %d, %d\n", k, q_group_state[tokens[k].mask[0]]);)
                        if (tokens[k].kind == REMIMU_KIND_CLOSE)
                        {
                            IF_VERBOSE(puts("!!~!~!~~~~!!~~!~   hit CLOSE. rewinding");)
                            // do nothing and continue on if we don't need this group
                            if (tokens[k].count_lo == 0 || q_group_accepts_zero[tokens[k].mask[0]])
                            {
                                IF_VERBOSE(puts("continuing because we don't need this group");)
                                q_group_state[tokens[k].mask[0]] = 0;
                                
                                if (!(tokens[k].mode & REMIMU_MODE_LAZY))
                                    q_group_stack[tokens[k].mask[0]] = 0;
                                
                                continue;
                            }
                            // otherwise go to the last point before the group
                            else
                            {
                                IF_VERBOSE(puts("going to last point before this group");)
                                _REWIND_OR_ABORT()
                                continue;
                            }
                        }
                        
                        assert(tokens[k].kind == REMIMU_KIND_OR);
                    }
                    
                    IF_VERBOSE(printf("--- FOUND ALTERNATION for paren at k %zd at k %d\n", orig_k, k);)
                    
                    ptrdiff_t k_diff = k - orig_k;
                    range_min = k_diff + 1;
                    
                    IF_VERBOSE(puts("(saving in paren after rewinding and looking for next regex token to check)");)
                    IF_VERBOSE(printf("%zd\n", range_min);)
                    _REWIND_DO_SAVE(k - k_diff)
                }
            }
            else if (tokens[k].kind == REMIMU_KIND_CLOSE)
            {
                // unquantified
                if (tokens[k].count_lo == 1 && tokens[k].count_hi == 2)
                {
                    // for captures
                    uint16_t cap_index = q_group_cap_index[tokens[k].mask[0]];
                    if (cap_index != 0xFFFF)
                        _REWIND_DO_SAVE_DUMMY(k)
                }
                // quantified
                else
                {
                    IF_VERBOSE(puts("closer test.....");)
                    if (!just_rewinded)
                    {
                        uint32_t prev = q_group_stack[tokens[k].mask[0]];
                        
                        IF_VERBOSE(printf("qrqrqrqrqrqrqrq-------      k %d, gs %d, gaz %d, i %zd, tklo %d, rmin %zd, tkhi %d, rmax %zd, prev %d, sn %d\n", k, q_group_state[tokens[k].mask[0]], q_group_accepts_zero[tokens[k].mask[0]], i, tokens[k].count_lo, range_min, tokens[k].count_hi, range_max, prev, stack_n);)
                        
                        range_max = tokens[k].count_hi;
                        range_max -= 1;
                        range_min = q_group_accepts_zero[tokens[k].mask[0]] ? 0 : tokens[k].count_lo;
                        //assert(q_group_state[tokens[k + tokens[k].pair_offset].mask[0]] <= i);
                        //if (prev) assert(rewind_stack[prev].i <= i);
                        IF_VERBOSE(printf("qzqzqzqzqzqzqzq-------      rmin %zd, rmax %zd\n", range_min, range_max);)
                        
                        // minimum requirement not yet met
                        if (q_group_state[tokens[k].mask[0]] + 1 < range_min)
                        {
                            IF_VERBOSE(puts("continuing minimum matches for a quantified group");)
                            q_group_state[tokens[k].mask[0]] += 1;
                            _REWIND_DO_SAVE(k)
                            
                            k += tokens[k].pair_offset; // back to start of group
                            k -= 1; // ensure we actually hit the group node next and not the node after it
                            continue;
                        }
                        // maximum allowance exceeded
                        else if (tokens[k].count_hi != 0 && q_group_state[tokens[k].mask[0]] + 1 > range_max)
                        {
                            IF_VERBOSE(printf("hit maximum allowed instances of a quantified group %d %zd\n", q_group_state[tokens[k].mask[0]], range_max);)
                            range_max -= 1;
                            _REWIND_OR_ABORT()
                            continue;
                        }
                        
                        // fallback case to detect zero-length matches when we backtracked into the inside of this group
                        // after an attempted parse of a second copy of itself
                        uint8_t force_zero = 0;
                        if (prev != 0 && (uint32_t)rewind_stack[prev].i > (uint32_t)i)
                        {
                            // find matching open paren
                            size_t n = stack_n - 1;
                            while (n > 0 && rewind_stack[n].k != k + tokens[k].pair_offset)
                                n -= 1;
                            assert(n > 0);
                            if (rewind_stack[n].i == i)
                                force_zero = 1;
                        }
                        
                        // reject zero-length matches
                        if ((force_zero || (prev != 0 && (uint32_t)rewind_stack[prev].i == (uint32_t)i))) //  && q_group_state[tokens[k].mask[0]] > 0
                        {
                            IF_VERBOSE(printf("rejecting zero-length match..... %d %zd %zd\n", force_zero, rewind_stack[prev].i, i);)
                            IF_VERBOSE(printf("%d (k: %d)\n", q_group_state[tokens[k].mask[0]], k);)
                            
                            q_group_accepts_zero[tokens[k].mask[0]] = 1;
                            _REWIND_OR_ABORT()
                            //range_max = q_group_state[tokens[k].mask[0]];
                            //range_min = 0;
                        }
                        else if (tokens[k].mode & REMIMU_MODE_LAZY) // lazy
                        {
                            IF_VERBOSE(printf("nidnfasidfnidfndifn-------      %d, %d, %zd\n", q_group_state[tokens[k].mask[0]], tokens[k].count_lo, range_min);)
                            if (prev)
                                IF_VERBOSE(printf("lazy doesn't think it's zero-length. prev i %zd vs i %zd (depth %d)\n", rewind_stack[prev].i, i, stack_n);)
                            // continue on to past the group; group retry is in rewind state
                            q_group_state[tokens[k].mask[0]] += 1;
                            _REWIND_DO_SAVE(k)
                            q_group_state[tokens[k].mask[0]] = 0;
                        }
                        else // greedy
                        {
                            IF_VERBOSE(puts("wahiwahi");)
                            // clear unwanted memory if possessive
                            if ((tokens[k].mode & REMIMU_MODE_POSSESSIVE))
                            {
                                uint32_t k2 = k;
                                
                                // special case for first, only rewind to (, not to )
                                if (q_group_state[tokens[k].mask[0]] == 0)
                                    k2 = k + tokens[k].pair_offset;
                                
                                if (stack_n == 0)
                                    return -1;
                                stack_n -= 1;
                                
                                while (stack_n > 0 && rewind_stack[stack_n].k != k2)
                                    stack_n -= 1;
                                
                                if (stack_n == 0)
                                    return -1;
                            }
                            // continue to next match if sane
                            if ((uint32_t)q_group_state[tokens[k + tokens[k].pair_offset].mask[0]] < (uint32_t)i)
                            {
                                IF_VERBOSE(puts("REWINDING FROM GREEDY NON-REWIND CLOSER");)
                                q_group_state[tokens[k].mask[0]] += 1;
                                _REWIND_DO_SAVE(k)
                                k += tokens[k].pair_offset; // back to start of group
                                k -= 1; // ensure we actually hit the group node next and not the node after it
                            }
                            else
                                IF_VERBOSE(puts("CONTINUING FROM GREEDY NON-REWIND CLOSER");)
                        }
                    }
                    else
                    {
                        IF_VERBOSE(puts("IN CLOSER REWIND!!!");)
                        just_rewinded = 0;
                        
                        if (tokens[k].mode & REMIMU_MODE_LAZY)
                        {
                            // lazy rewind: need to try matching the group again
                            _REWIND_DO_SAVE_DUMMY(k)
                            q_group_stack[tokens[k].mask[0]] = stack_n;
                            k += tokens[k].pair_offset; // back to start of group
                            k -= 1; // ensure we actually hit the group node next and not the node after it
                        }
                        else
                        {
                            // greedy. if we're going to go outside the acceptable range, rewind
                            IF_VERBOSE(printf("kufukufu %d %zd\n", tokens[k].count_lo, range_min);)
                            //uint64_t old_i = i;
                            if (q_group_state[tokens[k].mask[0]] < range_min && !q_group_accepts_zero[tokens[k].mask[0]])
                            {
                                IF_VERBOSE(printf("rewinding from greedy group because we're going to go out of range (%d vs %zd)\n", q_group_state[tokens[k].mask[0]], range_min);)
                                //i = old_i;
                                _REWIND_OR_ABORT()
                            }
                            // otherwise continue on to past the group
                            else
                            {
                                IF_VERBOSE(puts("continuing past greedy group");)
                                q_group_state[tokens[k].mask[0]] = 0;
                                
                                // for captures
                                uint16_t cap_index = q_group_cap_index[tokens[k].mask[0]];
                                if (cap_index != 0xFFFF)
                                    _REWIND_DO_SAVE_DUMMY(k)
                            }
                        }
                    }
                }
            }
            else if (tokens[k].kind == REMIMU_KIND_OR)
            {
                IF_VERBOSE(printf("hit OR at %d. adding %d\n", k, tokens[k].pair_offset);)
                k += tokens[k].pair_offset;
                k -= 1;
            }
            else if (tokens[k].kind == REMIMU_KIND_NORMAL)
            {
                if (!just_rewinded)
                {
                    uint64_t n = 0;
                    // do whatever the obligatory minimum amount of matching is
                    uint64_t old_i = i;
                    while (n < tokens[k].count_lo && text[i] != 0 && _REGEX_CHECK_MASK(k, text[i]))
                    {
                        i += 1;
                        n += 1;
                    }
                    if (n < tokens[k].count_lo)
                    {
                        IF_VERBOSE(printf("non-match A. rewinding (token %d)\n", k);)
                        i = old_i;
                        _REWIND_OR_ABORT()
                        continue;
                    }
                    
                    if (tokens[k].mode & REMIMU_MODE_LAZY)
                    {
                        range_min = n;
                        range_max = tokens[k].count_hi - 1;
                        _REWIND_DO_SAVE(k)
                    }
                    else
                    {
                        uint64_t _limit = tokens[k].count_hi;
                        if (_limit == 0)
                            _limit = ~_limit;
                        range_min = n;
                        while (text[i] != 0 && _REGEX_CHECK_MASK(k, text[i]) && n + 1 < _limit)
                        {
                            IF_VERBOSE(printf("match!! (%c)\n", text[i]);)
                            i += 1;
                            n += 1;
                        }
                        range_max = n;
                        IF_VERBOSE(printf("set rmin to %zd and rmax to %zd on entry into normal greedy token with k %d\n", range_min, range_max, k);)
                        if (!(tokens[k].mode & REMIMU_MODE_POSSESSIVE))
                            _REWIND_DO_SAVE(k)
                    }
                }
                else
                {
                    just_rewinded = 0;
                    
                    if (tokens[k].mode & REMIMU_MODE_LAZY)
                    {
                        uint64_t _limit = range_max;
                        if (_limit == 0)
                            _limit = ~_limit;
                        
                        if (_REGEX_CHECK_MASK(k, text[i]) && text[i] != 0 && range_min < _limit)
                        {
                            IF_VERBOSE(printf("match2!! (%c) (k: %d)\n", text[i], k);)
                            i += 1;
                            range_min += 1;
                            _REWIND_DO_SAVE(k)
                        }
                        else
                        {
                            IF_VERBOSE(printf("core rewind lazy (k: %d)\n", k);)
                            _REWIND_OR_ABORT()
                        }
                    }
                    else
                    {
                        //IF_VERBOSE(printf("comparing rmin %zd and rmax %zd token with k %d\n", range_min, range_max, k);)
                        if (range_max > range_min)
                        {
                            IF_VERBOSE(printf("greedy normal going back (k: %d)\n", k);)
                            i -= 1;
                            range_max -= 1;
                            _REWIND_DO_SAVE(k)
                        }
                        else
                        {
                            IF_VERBOSE(printf("core rewind greedy (k: %d)\n", k);)
                            _REWIND_OR_ABORT()
                        }
                    }
                }
            }
            else
            {
                fprintf(stderr, "unimplemented token kind %d\n", tokens[k].kind);
                assert(0);
            }
        }
        //printf("k... %d\n", k);
    }
    
    if (caps != 0)
    {
        //printf("stack_n: %d\n", stack_n);
        fflush(stdout);
        for (size_t n = 0; n < stack_n; n++)
        {
            RegexMatcherState s = rewind_stack[n];
            int kind = tokens[s.k].kind;
            if (kind == REMIMU_KIND_OPEN || kind == REMIMU_KIND_CLOSE)
            {
                uint16_t cap_index = q_group_cap_index[tokens[s.k].mask[0]];
                if (cap_index == 0xFFFF)
                    continue;
                if (tokens[s.k].kind == REMIMU_KIND_OPEN)
                    cap_pos[cap_index] = s.i;
                else if (cap_pos[cap_index] >= 0)
                    cap_span[cap_index] = s.i - cap_pos[cap_index];
            }
        }
        // re-deinitialize capture positions that have no associated capture span
        for (size_t n = 0; n < caps; n++)
        {
            if (cap_span[n] == -1)
                cap_pos[n] = -1;
        }
    }
    
    #undef _REWIND_DO_SAVE
    #undef _REWIND_OR_ABORT
    #undef _REGEX_CHECK_IS_W
    #undef _P_TEXT_HIGHLIGHTED
    #undef IF_VERBOSE
    
    return i;
}

static inline void print_regex_tokens(RegexToken * tokens)
{
    const char * kind_to_str[] = {
        "NORMAL",
        "OPEN",
        "NCOPEN",
        "CLOSE",
        "OR",
        "CARET",
        "DOLLAR",
        "BOUND",
        "NBOUND",
        "END",
    };
    const char * mode_to_str[] = {
        "GREEDY",
        "POSSESS",
        "LAZY",
    };
    for (int k = 0;; k++)
    {
        printf("%s\t%s\t", kind_to_str[tokens[k].kind], mode_to_str[tokens[k].mode]);
        
        int c_old = -1;
        for (int c = 0; c < (tokens[k].kind ? 0 : 256); c++)
        {
            #define _PRINT_C_SMART(c) { \
                if (c >= 0x20 && c <= 0x7E) \
                    printf("%c", c); \
                else \
                    printf("\\x%02x", c); \
            }
                
            if (_REGEX_CHECK_MASK(k, c))
            {
                if (c_old == -1)
                    c_old = c;
            }
            else if (c_old != -1)
            {
                if (c - 1 == c_old)
                {
                    _PRINT_C_SMART(c_old)
                    c_old = -1;
                }
                else if (c - 2 == c_old)
                {
                    _PRINT_C_SMART(c_old)
                    _PRINT_C_SMART(c_old + 1)
                    c_old = -1;
                }
                else
                {
                    _PRINT_C_SMART(c_old)
                    printf("-");
                    _PRINT_C_SMART(c - 1)
                    c_old = -1;
                }
            }
        }
        
        /*
        printf("\t");
        for (int i = 0; i < 16; i++)
            printf("%04x", tokens[k].mask[i]);
        */
        
        printf("\t{%d,%d}\t(%d)\n", tokens[k].count_lo, tokens[k].count_hi - 1, tokens[k].pair_offset);
        
        if (tokens[k].kind == REMIMU_KIND_END)
            break;
    }
}

#undef _REGEX_CHECK_MASK

#endif //INCLUDE_REMIMU
