#include "test.h"
#include <gtest/gtest.h>

#include <game/prng.h>

// https://www.pcg-random.org/using-pcg-c-basic.html, retrieved 2020-05-25
// suggests to use `pcg-32-global.demo.c`:
//
// ```
// unix% ./pcg32-demo
// pcg32_random_r:
//       -  result:      32-bit unsigned int (uint32_t)
//       -  period:      2^64   (* 2^63 streams)
//       -  state type:  pcg32_random_t (16 bytes)
//       -  output func: XSH-RR
//
// Round 1:
//   32bit: 0xa15c02b7 0x7b47f409 0xba1d3330 0x83d2f293 0xbfa4784b 0xcbed606e
//   Coins: HHTTTHTHHHTHTTTHHHHHTTTHHHTHTHTHTTHTTTHHHHHHTTTTHHTTTTTHTTTTTTTHT
//   Rolls: 3 4 1 1 2 2 3 2 4 3 2 4 3 3 5 2 3 1 3 1 5 1 4 1 5 6 4 6 6 2 6 3 3
//   Cards: Qd Ks 6d 3s 3d 4c 3h Td Kc 5c Jh Kd Jd As 4s 4h Ad Th Ac Jc 7s Qs
//          2s 7h Kh 2d 6c Ah 4d Qh 9h 6s 5s 2c 9c Ts 8d 9s 3c 8c Js 5d 2h 6h
//          7d 8s 9d 5h 8h Qc 7c Tc
// ⋮
// ⋮
// Round 5:
//   32bit: 0xfcef7cd6 0x1b488b5a 0xd0daf7ea 0x1d9a70f7 0x241a37cf 0x9a3857b7
//   Coins: HHHHTHHTTHTTHHHTTTHHTHTHTTTTHTTHTHTTTHHHTHTHTTHTTHTHHTHTHHHTHTHTT
//   Rolls: 5 4 1 2 6 1 3 1 5 6 3 6 2 1 4 4 5 2 1 5 6 5 6 4 4 4 5 2 6 4 3 5 6
//   Cards: 4d 9s Qc 9h As Qs 7s 4c Kd 6h 6s 2c 8c 5d 7h 5h Jc 3s 7c Jh Js Ks
//          Tc Jd Kc Th 3h Ts Qh Ad Td 3c Ah 2d 3d 5c Ac 8s 5s 9c 2h 6c 6d Kh
//          Qd 8d 7d 2s 8h 4h 9d 4s
// ```
//
// Numbers can also be seen at
// https://github.com/imneme/pcg-c/blob/83252d9c23df9c82ecb42210afed61a7b42402d7/test-high/expected/check-pcg32.out.
//
// It's also the output of `pcg32-global-demo.c` from
// https://github.com/imneme/pcg-c-basic/tree/bc39cd76ac3d541e618606bcc6e1e5ba5e5e6aa3.
//
// Only the first 6 numbers are taken from the generator directly, after this,
// something more complicated is done.

static const unsigned int PCG32_GLOBAL_DEMO[] = {
	0xa15c02b7,
	0x7b47f409,
	0xba1d3330,
	0x83d2f293,
	0xbfa4784b,
	0xcbed606e,
};

TEST(Prng, EqualsPcg32GlobalDemo)
{
	uint64_t aSeed[2] = {42, 54};

	CPrng Prng;
	Prng.Seed(aSeed);
	for(auto Expected : PCG32_GLOBAL_DEMO)
	{
		EXPECT_EQ(Prng.RandomBits(), Expected);
	}
}

TEST(Prng, Description)
{
	CPrng Prng;
	EXPECT_STREQ(Prng.Description(), "pcg-xsh-rr:unseeded");

	uint64_t aSeed0[2] = {0xfedbca9876543210, 0x0123456789abcdef};
	uint64_t aSeed1[2] = {0x0123456789abcdef, 0xfedcba9876543210};
	uint64_t aSeed2[2] = {0x0000000000000000, 0x0000000000000000};

	Prng.Seed(aSeed0);
	EXPECT_STREQ(Prng.Description(), "pcg-xsh-rr:fedbca9876543210:0123456789abcdef");
	Prng.Seed(aSeed1);
	EXPECT_STREQ(Prng.Description(), "pcg-xsh-rr:0123456789abcdef:fedcba9876543210");
	Prng.Seed(aSeed2);
	EXPECT_STREQ(Prng.Description(), "pcg-xsh-rr:0000000000000000:0000000000000000");
}
