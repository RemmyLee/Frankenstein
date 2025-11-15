#include "mapper.h"

using namespace Frankenstein;

/**********************************************/
/***************** MAPPER 0 *******************/
/**********************************************/

// Mapper 0 (NROM): No bank switching
// PRG-ROM: 16KB or 32KB at 0x8000-0xFFFF (16KB is mirrored)
// CHR-ROM: 8KB at 0x0000-0x1FFF
// SRAM: 8KB at 0x6000-0x7FFF

u8 Mapper0::Read(u16 address)
{
    if (address < 0x2000) {
        // CHR-ROM: 8KB
        return rom.GetCHR()[address];
    } else if (address >= 0x8000) {
        // PRG-ROM: Handle both 16KB (mirrored) and 32KB
        u16 index;
        if (prgBanks == 1) {
            // 16KB ROM: mirror to fill 32KB space
            index = (address - 0x8000) & 0x3FFF;  // Mask to 16KB
        } else {
            // 32KB ROM: direct mapping
            index = address - 0x8000;
        }
        return rom.GetPRG()[index];
    } else if (address >= 0x6000) {
        // SRAM
        return rom.GetSRAM()[address - 0x6000];
    }
    return 0;
}

void Mapper0::Write(u16 address, u8 value)
{
    if (address < 0x2000) {
        // CHR-RAM write (if cartridge has RAM instead of ROM)
        rom.GetCHR()[address] = value;
    } else if (address >= 0x6000 && address < 0x8000) {
        // SRAM write
        rom.GetSRAM()[address - 0x6000] = value;
    }
    // PRG-ROM area (0x8000-0xFFFF): writes are ignored (it's ROM)
}

void Mapper0::Step()
{
    // No IRQ or special timing logic for Mapper 0
}

Mapper0::Mapper0(Rom& pRom)
    : rom(pRom)
{
    prgBanks = rom.GetHeader().prgRomBanks;
}

Mapper0::~Mapper0() {}

/**********************************************/
/***************** MAPPER 1 *******************/
/**********************************************/

u8 Mapper1::Read(u16 address)
{
    if (address < 0x2000) {
        u16 bank = address / 0x1000;
        u16 offset = address % 0x1000;
        return rom.GetCHR()[chrOffsets[bank] + offset];
    } else if (address >= 0x8000) {
        address -= 0x8000;
        u16 bank = address / 0x4000;
        u16 offset = address % 0x4000;
        return rom.GetPRG()[prgOffsets[bank] + offset];
    } else if (address >= 0x6000) {
        return rom.GetSRAM()[address - 0x6000];
    }
    return 0;
}

void Mapper1::Write(u16 address, u8 value)
{
    if (address < 0x2000) {
        u16 bank = address / 0x1000;
        u16 offset = address % 0x1000;
        rom.GetCHR()[chrOffsets[bank] + offset] = value;
    } else if (address >= 0x8000) {
        loadRegister(address, value);
    } else if (address >= 0x6000) {
        rom.GetSRAM()[address - 0x6000] = value;
    }
}

void Mapper1::Step()
{
    //do nothing
}

void Mapper1::loadRegister(u16 address, u8 value)
{
    if ((value & 0x80) == 0x80) {
        shiftRegister = 0x10;
        writeControl(control | 0x0C);
    } else {
        bool complete = (shiftRegister & 1) == 1;
        shiftRegister >>= 1;
        shiftRegister |= (value & 1) << 4;
        if (complete) {
            writeRegister(address, shiftRegister);
            shiftRegister = 0x10;
        }
    }
}

void Mapper1::writeRegister(u16 address, u8 value)
{
    if (address <= 0x9FFF) {
        writeControl(value);
    } else if (address <= 0xBFFF) {
        writeCHRBank0(value);
    } else if (address <= 0xDFFF) {
        writeCHRBank1(value);
    } else if (address <= 0xFFFF) {
        writePRGBank(value);
    }
}

// Control (internal, $8000-$9FFF)
void Mapper1::writeControl(u8 value)
{
    control = value;
    chrMode = (value >> 4) & 1;
    prgMode = (value >> 2) & 3;
    u8 mirror = value & 3;
    switch (mirror) {
    case 0:
        mirrorMode = MirrorSingle0;
        break;
    case 1:
        mirrorMode = MirrorSingle1;
        break;
    case 2:
        mirrorMode = MirrorVertical;
        break;
    case 3:
        mirrorMode = MirrorHorizontal;
        break;
    }
    updateOffsets();
}

// CHR bank 0 (internal, $A000-$BFFF)
void Mapper1::writeCHRBank0(u8 value)
{
    chrBank0 = value;
    updateOffsets();
}

// CHR bank 1 (internal, $C000-$DFFF)
void Mapper1::writeCHRBank1(u8 value)
{
    chrBank1 = value;
    updateOffsets();
}

// PRG bank (internal, $E000-$FFFF)
void Mapper1::writePRGBank(u8 value)
{
    prgBank = value & 0x0F;
    updateOffsets();
}

s32 Mapper1::prgBankOffset(s32 index)
{
    if (index >= 0x80) {
        index -= 0x100;
    }
    u32 PRGSize = rom.GetHeader().prgRomBanks * PRGROM_BANK_SIZE;
    index %= PRGSize / 0x4000;
    s32 offset = index * 0x4000;
    if (offset < 0) {
        offset += PRGSize;
    }
    return offset;
}

s32 Mapper1::chrBankOffset(s32 index)
{
    if (index >= 0x80) {
        index -= 0x100;
    }
    u32 CHRSize = rom.GetHeader().vRomBanks * VROM_BANK_SIZE;
    index %= CHRSize / 0x1000;
    s32 offset = index * 0x1000;
    if (offset < 0) {
        offset += CHRSize;
    }
    return offset;
}

// PRG ROM bank mode (0, 1: switch 32 KB at $8000, ignoring low bit of bank number;
//                    2: fix first bank at $8000 and switch 16 KB bank at $C000;
//                    3: fix last bank at $C000 and switch 16 KB bank at $8000)
// CHR ROM bank mode (0: switch 8 KB at a time; 1: switch two separate 4 KB banks)
void Mapper1::updateOffsets()
{
    switch (prgMode) {
    case 0:
    case 1:
        prgOffsets[0] = prgBankOffset(prgBank & 0xFE);
        prgOffsets[1] = prgBankOffset(prgBank | 0x01);
        break;
    case 2:
        prgOffsets[0] = 0;
        prgOffsets[1] = prgBankOffset(prgBank);
        break;
    case 3:
        prgOffsets[0] = prgBankOffset(prgBank);
        prgOffsets[1] = prgBankOffset(-1);
        break;
    }
    switch (chrMode) {
    case 0:
        chrOffsets[0] = chrBankOffset(chrBank0 & 0xFE);
        chrOffsets[1] = chrBankOffset(chrBank0 | 0x01);
        break;
    case 1:
        chrOffsets[0] = chrBankOffset(chrBank0);
        chrOffsets[1] = chrBankOffset(chrBank1);
        break;
    }
}

Mapper1::Mapper1(Rom& pRom)
    : rom(pRom)
{
    shiftRegister = 0x10;
    prgOffsets[1] = prgBankOffset(-1);
}

Mapper1::~Mapper1() {}

/**********************************************/
/***************** MAPPER 2 *******************/
/**********************************************/

u8 Mapper2::Read(u16 address)
{
    u16 index;
    if (address < 0x2000) {
        return rom.GetCHR()[address];
    } else if (address >= 0xC000) {
        index = prgBank2 * 0x4000 + (address - 0xC000);
        return rom.GetPRG()[index];
    } else if (address >= 0x8000) {
        index = prgBank1 * 0x4000 + (address - 0x8000);
        return rom.GetPRG()[index];
    } else if (address >= 0x6000) {
        index = address - 0x6000;
        return rom.GetSRAM()[index];
    } else {
        return 0;
    }
}

void Mapper2::Write(u16 address, u8 value)
{
    u16 index;
    if (address < 0x2000) {
        rom.GetCHR()[address] = value;
    } else if (address >= 0x8000) {
        prgBank1 = value % prgBanks;
    } else if (address >= 0x6000) {
        index = address - 0x6000;
        rom.GetSRAM()[index] = value;
    }
}

void Mapper2::Step()
{
    //nothing to do
}

Mapper2::Mapper2(Rom& pRom)
    : rom(pRom)
{
    prgBanks = rom.GetHeader().prgRomBanks;
    prgBank1 = 0;
    prgBank2 = prgBanks - 1;
}

Mapper2::~Mapper2() {}

/**********************************************/
/***************** MAPPER 3 *******************/
/**********************************************/

// Mapper 3 (CNROM): Bank-switched CHR-ROM
// PRG-ROM: Fixed 16KB or 32KB (no banking)
// CHR-ROM: Switchable 8KB banks (selected by writes to 0x8000-0xFFFF)

u8 Mapper3::Read(u16 address)
{
    if (address < 0x2000) {
        // CHR-ROM: 8KB bank-switched
        u32 index = chrBank * 0x2000 + address;
        return rom.GetCHR()[index];
    } else if (address >= 0x8000) {
        // PRG-ROM: Fixed (16KB or 32KB)
        u16 index;
        if (prgBanks == 1) {
            // 16KB: mirror to fill 32KB
            index = (address - 0x8000) & 0x3FFF;
        } else {
            // 32KB: direct mapping
            index = address - 0x8000;
        }
        return rom.GetPRG()[index];
    } else if (address >= 0x6000) {
        // SRAM
        return rom.GetSRAM()[address - 0x6000];
    }
    return 0;
}

void Mapper3::Write(u16 address, u8 value)
{
    if (address < 0x2000) {
        // CHR-RAM write (some games use RAM instead of ROM)
        u32 index = chrBank * 0x2000 + address;
        rom.GetCHR()[index] = value;
    } else if (address >= 0x8000) {
        // Select CHR bank (only lower 2 bits typically used)
        chrBank = value & (chrBanks - 1);
    } else if (address >= 0x6000) {
        // SRAM write
        rom.GetSRAM()[address - 0x6000] = value;
    }
}

void Mapper3::Step()
{
    // No IRQ or special timing
}

Mapper3::Mapper3(Rom& pRom)
    : rom(pRom)
    , chrBank(0)
{
    prgBanks = rom.GetHeader().prgRomBanks;
    chrBanks = rom.GetHeader().vRomBanks;
    if (chrBanks == 0) {
        chrBanks = 1;  // CHR-RAM if no CHR-ROM
    }
}

Mapper3::~Mapper3() {}

/**********************************************/
/***************** MAPPER 4 *******************/
/**********************************************/

u8 Mapper4::Read(u16 address)
{
    return 0;
}

void Mapper4::Write(u16 address, u8 value)
{
}

void Mapper4::Step()
{
}

Mapper4::~Mapper4() {}

/**********************************************/
/***************** MAPPER 7 *******************/
/**********************************************/

// Mapper 7 (AxROM): 32KB PRG bank switching
// PRG-ROM: Switchable 32KB banks at 0x8000-0xFFFF
// CHR-RAM: Fixed 8KB (not ROM)
// Mirroring: Single-screen (controlled by bit 4)

u8 Mapper7::Read(u16 address)
{
    if (address < 0x2000) {
        // CHR-RAM: Fixed 8KB
        return rom.GetCHR()[address];
    } else if (address >= 0x8000) {
        // PRG-ROM: Switchable 32KB bank
        u32 index = prgBank * 0x8000 + (address - 0x8000);
        return rom.GetPRG()[index];
    } else if (address >= 0x6000) {
        // SRAM
        return rom.GetSRAM()[address - 0x6000];
    }
    return 0;
}

void Mapper7::Write(u16 address, u8 value)
{
    if (address < 0x2000) {
        // CHR-RAM write
        rom.GetCHR()[address] = value;
    } else if (address >= 0x8000) {
        // Select 32KB PRG bank (bits 0-2)
        prgBank = value & 0x07;
        if (prgBank >= prgBanks) {
            prgBank = prgBanks - 1;
        }
        // Bit 4 controls single-screen mirroring
        // (mirroring would be handled by PPU - not implemented here)
    } else if (address >= 0x6000) {
        // SRAM write
        rom.GetSRAM()[address - 0x6000] = value;
    }
}

void Mapper7::Step()
{
    // No IRQ or special timing
}

Mapper7::Mapper7(Rom& pRom)
    : rom(pRom)
    , prgBank(0)
{
    prgBanks = rom.GetHeader().prgRomBanks / 2;  // 32KB banks
    if (prgBanks == 0) {
        prgBanks = 1;
    }
}

Mapper7::~Mapper7() {}
