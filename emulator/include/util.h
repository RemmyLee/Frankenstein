#pragma once

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;

#define KILOBYTE 1024

/**
 * Check whether the bit Position is set in src.
 * Position is 1-based.
 */
template <unsigned int Position, typename T = u8>
inline bool CheckBit(T src) {
    static_assert((Position-1) <= (sizeof(T) * 8) && Position >= 1,
            "Position must be lower than sizeof(T) and greater or equals to 1");

    int position = Position - 1;
    return (src & (1 << position )) >> position;
}

/**
 * Check whether the bit pos is set.
 * pos is 1-based.
 */
template<typename T = u8>
inline bool CheckBit(T src, u8 pos) {
    int position = pos - 1;
    return (src & (1 << position )) >> position;
}

/**
 * Set the Position bit in src.
 * Position is 1-based.
 */
template <unsigned int Position, typename T = u8>
inline void SetBit(T& src) {
    static_assert((Position-1) <= (sizeof(T) * 8) && Position >= 1,
            "Position must be lower than sizeof(T) and greater or equals to 1");

    int position = Position - 1;
    src |= (1 << position);
}

/**
 * Set the pos bit in src.
 * pos is 1-based.
 */
template<typename T = u8>
inline void SetBit(T src, u8 pos) {
    int position = pos - 1;
    src |= (1 << position);
}

/**
 * Clear the Position bit in src.
 * Position is 1-based.
 */
template <unsigned int Position, typename T = u8>
inline void ClearBit(T& src) {
    static_assert((Position-1) <= (sizeof(T) * 8) && Position >= 1,
            "Position must be lower than sizeof(T) and greater or equals to 1");

    int position = Position - 1;
    src &= ~(1 << position);
}

/**
 * Clear the pos bit in src.
 * pos is 1-based.
 */
template<typename T = u8>
inline void ClearBit(T src, u8 pos) {
    int position = pos - 1;
    src &= ~(1 << position);
}

/**
 * Set or clear the Position bit in src based on value.
 * Position is 1-based.
 */
template <unsigned int Position, typename T = u8>
inline void AssignBit(T& src, bool value) {
    static_assert((Position-1) <= (sizeof(T) * 8) && Position >= 1,
            "Position must be lower than sizeof(T) and greater or equals to 1");

    int position = Position - 1;
    src ^= (-((int)value) ^ src) & (1 << position);
}


template <typename T = u8>
inline bool CheckOverflow(T src, T value, T result, bool adc) {
    if (adc) {
        return !((src ^ value) & 0x80) && ((src ^ result) & 0x80);
    } else {
        return ((src ^ value) & 0x80) && ((src ^ result) & 0x80);
    }
}

/**
 * Return true when src is negative (has bit 8 set)
 */
template <typename T = u8>
inline bool CheckSign(T& src){
    return CheckBit<8, T>(src);
}

/**
 * Return true when src is zero.
 */
template <typename T = u8>
inline bool CheckZero(T src){
    return src == 0;
}


//sizes related to hardware (in bytes) :
const u16 NES_PAGE_SIZE = 256;
const u32 PRGROM_BANK_SIZE = 16 * KILOBYTE;
const u32 VROM_BANK_SIZE = 8 * KILOBYTE;
const u32 PRGRAM_BANK_SIZE = 8 * KILOBYTE;
const u8 TILE_SIZE = 16;
const u8 BLOCK_SIZE = 64;
