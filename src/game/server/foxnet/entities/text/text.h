#ifndef GAME_SERVER_FOXNET_ENTITES_TEXT_TEXT_H
#define GAME_SERVER_FOXNET_ENTITES_TEXT_TEXT_H

#include <game/server/entity.h>
#include <game/server/gameworld.h>
#include <game/server/gamecontext.h>

#include <base/vmath.h>

#include <vector>

#define MAX_TEXT_LEN 32

constexpr int GlyphH = 5;
constexpr int GlyphW = 3;

static const bool asciiTable[256][GlyphH][GlyphW] = {
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 0
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 1
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 2
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 3
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 4
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 5
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 6
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 7
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 8
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 9
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 10
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 11
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 12
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 13
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 14
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 15
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 16
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 17
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 18
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 19
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 20
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 21
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 22
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 23
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 24
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 25
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 26
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 27
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 28
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 29
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 30
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 31
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 32
	{{false, true, false}, {false, true, false}, {false, true, false}, {false, false, false}, {false, true, false}}, // ascii 33 !
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 34
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 35
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 36
	{{true, false, true}, {true, false, false}, {false, true, false}, {false, false, true}, {true, false, true}}, // ascii 37 %
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 38
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 39
	{{false, true, false}, {true, false, false}, {true, false, false}, {true, false, false}, {false, true, false}}, // ascii 4false (
	{{false, true, false}, {false, false, true}, {false, false, true}, {false, false, true}, {false, true, false}}, // ascii 4true )
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 42
	{{false, false, false}, {false, true, false}, {true, true, true}, {false, true, false}, {false, false, false}}, // ascii 43 +
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, true, false}, {true, false, false}}, // ascii 44 ,
	{{false, false, false}, {false, false, false}, {true, true, true}, {false, false, false}, {false, false, false}}, // ascii 45 -
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, true, false}}, // ascii 46 .
	{{false, false, false}, {false, false, true}, {false, true, false}, {true, false, false}, {false, false, false}}, // ascii 47 /
	{{true, true, true}, {true, false, true}, {true, false, true}, {true, false, true}, {true, true, true}}, // false
	{{false, true, false}, {true, true, false}, {false, true, false}, {false, true, false}, {false, true, false}}, // true
	{{true, true, true}, {false, false, true}, {true, true, true}, {true, false, false}, {true, true, true}}, // 2
	{{true, true, true}, {false, false, true}, {true, true, true}, {false, false, true}, {true, true, true}}, // 3
	{{true, false, true}, {true, false, true}, {true, true, true}, {false, false, true}, {false, false, true}}, // 4
	{{true, true, true}, {true, false, false}, {true, true, true}, {false, false, true}, {true, true, true}}, // 5
	{{true, true, true}, {true, false, false}, {true, true, true}, {true, false, true}, {true, true, true}}, // 6
	{{true, true, true}, {false, false, true}, {false, false, true}, {false, false, true}, {false, false, true}}, // 7
	{{true, true, true}, {true, false, true}, {true, true, true}, {true, false, true}, {true, true, true}}, // 8
	{{true, true, true}, {true, false, true}, {true, true, true}, {false, false, true}, {true, true, true}}, // 9
	{{false, false, false}, {false, true, false}, {false, false, false}, {false, true, false}, {false, false, false}}, // :
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 59
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 6false
	{{false, false, false}, {true, true, true}, {false, false, false}, {true, true, true}, {false, false, false}}, // ascii 6true =
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 62
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 63
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 64
	{{true, true, true}, {true, false, true}, {true, true, true}, {true, false, true}, {true, false, true}}, // A
	{{true, false, false}, {true, false, false}, {true, true, true}, {true, false, true}, {true, true, true}}, // B
	{{true, true, true}, {true, false, false}, {true, false, false}, {true, false, false}, {true, true, true}}, // C
	{{false, false, true}, {false, false, true}, {true, true, true}, {true, false, true}, {true, true, true}}, // D
	{{true, true, true}, {true, false, false}, {true, true, true}, {true, false, false}, {true, true, true}}, // E
	{{true, true, true}, {true, false, false}, {true, true, true}, {true, false, false}, {true, false, false}}, // F
	{{false, true, true}, {true, false, false}, {true, true, true}, {true, false, true}, {false, true, true}}, // G
	{{true, false, true}, {true, false, true}, {true, true, true}, {true, false, true}, {true, false, true}}, // H
	{{true, true, true}, {false, true, false}, {false, true, false}, {false, true, false}, {true, true, true}}, // I
	{{false, false, true}, {false, false, true}, {true, false, true}, {true, false, true}, {true, true, true}}, // J
	{{true, false, false}, {true, false, false}, {true, false, true}, {true, true, false}, {true, false, true}}, // K
	{{true, false, false}, {true, false, false}, {true, false, false}, {true, false, false}, {true, true, true}}, // L
	{{true, false, true}, {true, true, true}, {true, true, true}, {true, false, true}, {true, false, true}}, // M
	{{false, false, false}, {false, false, false}, {true, true, true}, {true, false, true}, {true, false, true}}, // N
	{{false, false, false}, {false, false, false}, {true, true, true}, {true, false, true}, {true, true, true}}, // O
	{{true, true, true}, {true, false, true}, {true, true, true}, {true, false, false}, {true, false, false}}, // P
	{{true, true, true}, {true, false, true}, {true, true, true}, {false, false, true}, {false, false, true}}, // Q
	{{false, false, false}, {true, false, true}, {true, true, false}, {true, false, false}, {true, false, false}}, // R
	{{false, true, true}, {true, false, false}, {false, true, false}, {false, false, true}, {true, true, false}}, // S
	{{true, true, true}, {false, true, false}, {false, true, false}, {false, true, false}, {false, true, false}}, // T
	{{false, false, false}, {false, false, false}, {true, false, true}, {true, false, true}, {true, true, true}}, // U
	{{false, false, false}, {false, false, false}, {true, false, true}, {true, true, true}, {false, true, false}}, // V
	{{true, false, true}, {true, false, true}, {true, true, true}, {true, true, true}, {true, true, true}}, // W
	{{false, false, false}, {false, false, false}, {true, false, true}, {false, true, false}, {true, false, true}}, // X
	{{true, false, true}, {true, false, true}, {true, true, true}, {false, true, false}, {false, true, false}}, // Y
	{{true, true, true}, {false, false, true}, {false, true, false}, {true, false, false}, {true, true, true}}, // Z
	{{true, true, false}, {true, false, false}, {true, false, false}, {true, false, false}, {true, true, false}}, // [
	{{false, false, false}, {true, false, false}, {false, true, false}, {false, false, true}, {false, false, false}}, // "\"
	{{false, true, true}, {false, false, true}, {false, false, true}, {false, false, true}, {false, true, true}}, // ]
	{{false, true, false}, {true, false, true}, {false, false, false}, {false, false, false}, {false, false, false}}, // ^
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {true, true, true}}, // _
	{{true, false, false}, {true, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // `
	{{false, false, false}, {false, false, false}, {false, true, true}, {true, false, true}, {true, true, true}}, // a
	{{true, false, false}, {true, false, false}, {true, true, true}, {true, false, true}, {true, true, true}}, // b
	{{false, false, false}, {false, false, false}, {true, true, true}, {true, false, false}, {true, true, true}}, // c
	{{false, false, true}, {false, false, true}, {true, true, true}, {true, false, true}, {true, true, true}}, // d
	{{false, false, false}, {false, false, false}, {false, true, false}, {true, false, true}, {false, true, true}}, // e
	{{false, false, true}, {false, true, false}, {true, true, true}, {false, true, false}, {false, true, false}}, // f
	{{false, false, false}, {true, true, true}, {true, false, true}, {false, true, true}, {true, true, true}}, // g
	{{true, false, false}, {true, false, false}, {true, true, true}, {true, false, true}, {true, false, true}}, // h
	{{false, true, false}, {false, false, false}, {false, true, false}, {false, true, false}, {false, true, false}}, // i
	{{false, true, false}, {false, false, false}, {false, true, false}, {false, true, false}, {true, true, false}}, // j
	{{true, false, false}, {true, false, false}, {true, false, true}, {true, true, false}, {true, false, true}}, // k
	{{true, false, false}, {true, false, false}, {true, false, false}, {true, false, false}, {true, true, false}}, // l
	{{false, false, false}, {false, false, false}, {true, true, false}, {true, true, true}, {true, true, true}}, // m
	{{false, false, false}, {false, false, false}, {true, true, false}, {true, false, true}, {true, false, true}}, // n
	{{false, false, false}, {false, false, false}, {true, true, true}, {true, false, true}, {true, true, true}}, // o
	{{false, false, false}, {true, true, true}, {true, false, true}, {true, true, true}, {true, false, false}}, // p
	{{false, false, false}, {true, true, true}, {true, false, true}, {true, true, true}, {false, false, true}}, // q
	{{false, false, false}, {false, false, false}, {true, true, false}, {true, false, true}, {true, false, false}}, // r
	{{false, false, false}, {false, false, false}, {true, true, false}, {true, true, true}, {false, true, true}}, // s
	{{false, true, false}, {true, true, true}, {false, true, false}, {false, true, false}, {false, true, false}}, // t
	{{false, false, false}, {false, false, false}, {true, false, true}, {true, false, true}, {true, true, true}}, // u
	{{false, false, false}, {false, false, false}, {true, false, true}, {true, false, true}, {false, true, false}}, // v
	{{false, false, false}, {false, false, false}, {true, false, true}, {true, true, true}, {true, true, true}}, // w
	{{false, false, false}, {false, false, false}, {true, false, true}, {false, true, false}, {true, false, true}}, // x
	{{false, false, false}, {false, false, false}, {true, false, true}, {false, true, false}, {true, false, false}}, // y
	{{false, false, false}, {false, false, false}, {true, true, true}, {false, true, false}, {true, true, true}}, // z
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 123
	{{false, true, false}, {false, true, false}, {false, true, false}, {false, true, false}, {false, true, false}}, // |
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 125
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 126
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 127
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 128
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 129
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 130
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 131
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 132
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 133
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 134
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 135
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 136
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 137
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 138
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 139
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 140
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 141
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 142
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 143
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 144
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 145
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 146
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 147
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 148
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 149
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 150
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 151
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 152
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 153
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 154
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 155
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 156
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 157
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 158
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 159
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 160
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 161
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 162
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 163
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 164
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 165
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 166
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 167
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 168
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 169
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 170
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 171
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 172
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 173
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 174
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 175
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 176
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 177
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 178
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 179
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 180
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 181
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 182
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 183
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 184
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 185
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 186
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 187
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 188
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 189
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 190
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 191
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 192
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 193
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 194
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 195
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 196
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 197
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 198
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 199
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 200
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 201
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 202
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 203
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 204
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 205
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 206
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 207
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 208
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 209
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 210
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 211
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 212
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 213
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 214
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 215
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 216
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 217
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 218
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 219
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 220
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 221
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 222
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 223
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 224
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 225
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 226
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 227
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 228
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 229
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 230
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 231
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 232
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 233
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 234
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 235
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 236
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 237
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 238
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 239
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 240
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 241
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 242
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 243
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 244
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 245
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 246
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 247
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 248
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 249
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 250
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 251
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 252
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 253
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}}, // ascii 254
	{{false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}, {false, false, false}} // ascii 255
};

struct FTextData
{
	int m_Id;
	vec2 m_Pos;
};


class CText : public CEntity
{
public:
	int m_AliveTicks;
	int m_CurTicks;
	int m_StartTick;

	int m_Owner;

	std::vector<FTextData *> m_pData;

	char m_aText[MAX_TEXT_LEN];

	void SetData(float Cell);

	CText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText, int EntType);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	virtual void DoSnap(int SnappingClient) {}
};

inline void CText::Reset()
{
	Server()->SnapFreeId(GetId());

	for(auto *pData : m_pData)
		Server()->SnapFreeId(pData->m_Id);

	GameWorld()->RemoveEntity(this);
}

inline void CText::Tick()
{
	if(++m_CurTicks - m_StartTick > m_AliveTicks)
		Reset();
}

inline void CText::Snap(int SnappingClient)
{
	DoSnap(SnappingClient);
}

inline void CText::SetData(float Cell)
{
	const int LetterGap = 1; // empty columns between letters
	const int Advance = GlyphW + LetterGap;

	vec2 Base = m_Pos;
	int Len = str_length(m_aText);

	for(int ci = 0; ci < Len && ci < MAX_TEXT_LEN; ++ci)
	{
		unsigned char ch = (unsigned char)m_aText[ci];
		int xOffsetCols = ci * Advance;

		for(int r = 0; r < GlyphH; ++r)
		{
			for(int c = 0; c < GlyphW; ++c)
			{
				if(!asciiTable[ch][r][c])
					continue;

				vec2 P = Base + vec2((xOffsetCols + c) * Cell, r * Cell);

				int Id = Server()->SnapNewId();

				FTextData *pD = new FTextData();
				pD->m_Id = Id;
				pD->m_Pos = P;
				m_pData.push_back(pD);
			}
		}
	}
}

inline CText::CText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText, int EntType) :
	CEntity(pGameWorld, EntType), m_Owner(Owner), m_AliveTicks(AliveTicks), m_CurTicks(0), m_StartTick(Server()->Tick())
{

}

class CProjectileText : public CText
{
	int m_Type;

public:
	CProjectileText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText, int Type = WEAPON_HAMMER);

	void Snap(int SnappingClient) override;
};

class CLaserText : public CText
{
public:
	CLaserText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText);

	void Snap(int SnappingClient) override;
};


#endif // GAME_SERVER_FOXNET_ENTITES_TEXT_TEXT_H
