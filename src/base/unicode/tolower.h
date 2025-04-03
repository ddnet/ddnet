#include <cstdint>

#if defined(__attribute__)
#define PACK __attribute__((packed))
#else
#define PACK
#endif

class PACK CUpperToLower
{
public:
	int32_t m_Upper;
	int32_t m_Lower;
};

extern const unsigned int ToLowerNum;

extern const CUpperToLower ToLowerMap[];
