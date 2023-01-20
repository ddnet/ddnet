/* (c) DDNet Team. See licence.txt in the root of the distribution for more information.     */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MASK_H
#define ENGINE_MASK_H

#include <cstdint>

struct CMask
{
	int64_t m_Mask;

	CMask()
	{
		m_Mask = -1LL; // all by default
	}

	CMask(const CMask &) = default;

	CMask(int ID)
	{
		m_Mask = 1LL << ID;
	}

	CMask(int64_t Mask)
	{
		m_Mask = Mask;
	}

	CMask operator~() const
	{
		return CMask(~m_Mask);
	}

	CMask operator^(CMask Mask)
	{
		return CMask(m_Mask ^ Mask.m_Mask);
	}

	CMask &operator=(CMask Mask)
	{
		m_Mask = Mask.m_Mask;
		return *this;
	}

	CMask operator|(CMask Mask)
	{
		return CMask(m_Mask | Mask.m_Mask);
	}

	void operator|=(CMask Mask)
	{
		m_Mask |= Mask.m_Mask;
	}

	CMask operator&(CMask Mask)
	{
		return CMask(m_Mask & Mask.m_Mask);
	}

	void operator&=(CMask Mask)
	{
		m_Mask &= Mask.m_Mask;
	}

	bool operator==(CMask Mask)
	{
		return m_Mask == Mask.m_Mask;
	}

	bool operator!=(CMask Mask)
	{
		return m_Mask != Mask.m_Mask;
	}
};

inline CMask CMaskAll() { return CMask(); }
inline CMask CMaskNone() { return ~CMask(); }
inline CMask CMaskOne(int ID) { return CMask(ID); }
inline CMask CMaskUnset(CMask Mask, int ID) { return Mask ^ CMaskOne(ID); }
inline CMask CMaskAllExceptOne(int ID) { return CMaskUnset(CMaskAll(), ID); }
inline bool CMaskIsSet(CMask Mask, int ID) { return (Mask & CMaskOne(ID)) != CMaskNone(); }

#endif
