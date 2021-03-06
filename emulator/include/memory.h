#pragma once

#include "util.h"

namespace Frankenstein {

static constexpr u16 ADDR_ZERO_PAGE = 0x0000;
static constexpr u16 ADDR_STACK = 0x0100;
static constexpr u16 ADDR_RAM = 0x0200;
static constexpr u16 ADDR_MIRROIR_1 = 0x0800;
static constexpr u16 ADDR_IO_REGISTERS_1 = 0x2000;
static constexpr u16 ADDR_MIRROIR_2 = 0x2008;
static constexpr u16 ADDR_IO_REGISTERS_2 = 0x4000;
static constexpr u16 ADDR_EXPANSION_ROM = 0x4020;
static constexpr u16 ADDR_SRAM = 0x6000;
static constexpr u16 ADDR_PRG_ROM_LOWER_BANK = 0x8000;
static constexpr u16 ADDR_PRG_ROM_UPPER_BANK = 0xC000;

enum class Addressing {
    Absolute,
    ZeroPage,
    Indexed,
    ZeroPageIndexed,
    Indirect,
    PreIndexedIndirect,
    PostIndexedIndirect,
};

class Nes;

template <typename DataType, typename AddressingType, unsigned int Size>
class Memory {
private:
    DataType raw[Size];
    Nes& nes;

    DataType Read(const AddressingType address);
    void Write(const AddressingType address, const DataType val);

public:
    // Memory proxy, allow transparent uses of Memory
    struct Ref {
        const AddressingType address;
        Memory* const outer;

    public:
        explicit Ref(const AddressingType& addr, Memory* const parent)
            : address(addr)
            , outer(parent)
        {
        }

        const Ref& operator=(const DataType& rhs) const
        {
            outer->Write(address, rhs);
            return *this;
        }

        operator const DataType() const
        {
            return outer->Read(address);
        }
    };

    explicit Memory(Nes& nes);

    Ref operator[](const AddressingType);

    void Copy(const DataType* source, const AddressingType destination, const unsigned int size);

    template <Addressing N>
    Ref Get(const DataType);

    template <Addressing N>
    Ref Get(const DataType, const DataType);

    template <Addressing N>
    Ref Get(const DataType, const DataType, const DataType);

    /**
     * Detects if two addresses are on the same page
     * @param startAddress the non-indexed address
     * @param endAddress the final address after addressing
     * @return if a page is crossed
     */
    static bool IsPageCrossed(AddressingType startAddress, AddressingType endAddress);

    /**
     * Build an address using only the least significative byte. The first half of
     * the address is assumed to be zero.
     *
     * @param low the least significant byte of the address
     * @return the two bytes address
     */
    static AddressingType FromValues(const DataType low);

    /**
     * Build an address using the most and the least significative bytes. Useful
     * because the memory is stored as little endian.
     *
     * @param low  the least significant byte of the address
     * @param high the most significant byte of the address
     * @return the two bytes address
     */
    static AddressingType FromValues(const DataType low, const DataType high);

    /**
     * Build the address [00][low].
     * @param low the least significant byte of the address
     * @return the builded address
     */
    static AddressingType ZeroPage(const DataType low);

    /**
     * Build the address [high][low].
     * @param low  the least significant byte of the address
     * @param high the most significant byte of the address
     * @return the builded address
     */
    static AddressingType Absolute(const DataType low, const DataType high);

    /**
     * Compute the complete address [high][low] + index
     * @param low   the least significant byte of the address
     * @param high  the most significant byte of the address
     * @param index  value to add to the constructed address
     * @return the computed address
     */
    static AddressingType Indexed(const DataType low, const DataType high, const DataType index);

    /**
     * Compute the ZeroPage address [00][low] + index.
     * @param low   the least significant byte of the address
     * @param index a value to add to the constructed address
     * @return the computed address
     */
    static AddressingType ZeroPageIndexed(const DataType low, const DataType index);

    /**
     * Fetch an address at memory [high][low].
     * @param  low  the least significant byte
     * @param  high the most significant byte
     * @return the stored address
     */
    AddressingType Indirect(const DataType low, const DataType high);

    /**
     * Fetch an address at memory [00][low + index]. If [low + index] overflow, only
     * the lowest byte is kept.
     *
     * @param  low   ZeroPage address of the memory containing the address
     * @param  index a value to add to low to get the actual address
     * @return the pre-indexed address
     */
    AddressingType PreIndexedIndirect(const DataType low, const DataType index);

    /**
     * Fetch an address at memory [00][low] then increase the address by index.
     * @param  low   ZeroPage address of the memory containing the address
     * @param  index an index to add to the loaded address. Usually a register [X|Y]
     * @return the post-indexed address
     */
    AddressingType PostIndexedIndirect(const DataType low, const DataType index);

    //u16 Implied();
    //u16 Accumulator();
    //u16 Relative();
};

}
