/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MASK_H
#define ENGINE_MASK_H

#include <cstdint>

struct CMask
{
	int64_t m_aMask;

	CMask()
	{
		m_aMask = -1LL; // all by default
	}

	CMask(const CMask &) = default;

	CMask(int ClientID)
	{
		m_aMask = 1LL << ClientID;
	}

	CMask(int64_t Mask)
	{
		m_aMask = Mask;
	}

	CMask operator~() const
	{
		return CMask(~m_aMask);
	}

	CMask operator^(CMask Mask)
	{
		return CMask(m_aMask ^ Mask.m_aMask);
	}

	CMask &operator=(CMask Mask)
	{
		m_aMask = Mask.m_aMask;
		return *this;
	}

	CMask operator|(CMask Mask)
	{
		return CMask(m_aMask | Mask.m_aMask);
	}

	void operator|=(CMask Mask)
	{
		m_aMask |= Mask.m_aMask;
	}

	CMask operator&(CMask Mask)
	{
		return CMask(m_aMask & Mask.m_aMask);
	}

	void operator&=(CMask Mask)
	{
		m_aMask &= Mask.m_aMask;
	}

	bool operator==(CMask Mask)
	{
		return m_aMask == Mask.m_aMask;
	}

	bool operator!=(CMask Mask)
	{
		return m_aMask != Mask.m_aMask;
	}
};
#endif
