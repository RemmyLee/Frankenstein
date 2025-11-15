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

// Mapper 4 (MMC3): Most complex and widely used mapper
// PRG-ROM: 4x 8KB banks (2 switchable, 2 fixed)
// CHR-ROM: 8x 1KB banks (all switchable)
// IRQ: Scanline counter for split-screen effects
// Games: Super Mario Bros 2/3, Kirby, Mega Man 3-6

u8 Mapper4::Read(u16 address)
{
    if (address < 0x2000) {
        // CHR-ROM: 8x 1KB banks
        u16 bank = address / 0x0400;  // Which 1KB bank
        u16 offset = address % 0x0400;
        return rom.GetCHR()[chrOffsets[bank] + offset];
    } else if (address >= 0x8000) {
        // PRG-ROM: 4x 8KB banks
        address -= 0x8000;
        u16 bank = address / 0x2000;  // Which 8KB bank
        u16 offset = address % 0x2000;
        return rom.GetPRG()[prgOffsets[bank] + offset];
    } else if (address >= 0x6000) {
        // SRAM: 8KB
        return rom.GetSRAM()[address - 0x6000];
    }
    return 0;
}

void Mapper4::Write(u16 address, u8 value)
{
    if (address < 0x2000) {
        // CHR-RAM write
        u16 bank = address / 0x0400;
        u16 offset = address % 0x0400;
        rom.GetCHR()[chrOffsets[bank] + offset] = value;
    } else if (address >= 0x8000) {
        // Register writes based on address bits
        if ((address & 0xE001) == 0x8000) {
            writeBankSelect(value);
        } else if ((address & 0xE001) == 0x8001) {
            writeBankData(value);
        } else if ((address & 0xE001) == 0xA000) {
            writeMirror(value);
        } else if ((address & 0xE001) == 0xA001) {
            writeProtect(value);
        } else if ((address & 0xE001) == 0xC000) {
            writeIRQLatch(value);
        } else if ((address & 0xE001) == 0xC001) {
            writeIRQReload(value);
        } else if ((address & 0xE001) == 0xE000) {
            writeIRQDisable(value);
        } else if ((address & 0xE001) == 0xE001) {
            writeIRQEnable(value);
        }
    } else if (address >= 0x6000) {
        // SRAM write
        rom.GetSRAM()[address - 0x6000] = value;
    }
}

void Mapper4::Step()
{
    // IRQ counter is clocked by PPU A12 rising edges
    // In practice, this happens once per scanline
    // This is called once per PPU cycle (3x per CPU cycle)
    // We need to detect scanline boundaries

    // Simplified implementation: Decrement counter
    if (irqCounter == 0) {
        if (irqReload) {
            irqCounter = irqLatch;
            irqReload = false;
        }
    } else {
        irqCounter--;
        if (irqCounter == 0 && irqEnabled) {
            // Trigger IRQ in CPU
            // TODO: Need access to CPU to set IRQ flag
            // For now, just note that IRQ should fire
        }
    }
}

// Bank select: Choose which bank register to update
void Mapper4::writeBankSelect(u8 value)
{
    bankSelect = value & 0x07;  // Bank register (0-7)
    prgMode = (value >> 6) & 0x01;  // PRG banking mode
    chrMode = (value >> 7) & 0x01;  // CHR banking mode
    updateOffsets();
}

// Bank data: Update selected bank register
void Mapper4::writeBankData(u8 value)
{
    bankRegisters[bankSelect] = value;
    updateOffsets();
}

// Mirroring control
void Mapper4::writeMirror(u8 value)
{
    mirrorMode = value & 0x01;
    // 0 = vertical, 1 = horizontal
}

// PRG RAM protect (not fully implemented)
void Mapper4::writeProtect(u8 value)
{
    // Bit 7: PRG RAM chip enable
    // Bit 6: Write protect
    // Not critical for most games
}

// IRQ latch: Set reload value
void Mapper4::writeIRQLatch(u8 value)
{
    irqLatch = value;
}

// IRQ reload: Reset counter
void Mapper4::writeIRQReload(u8 value)
{
    irqReload = true;
}

// IRQ disable
void Mapper4::writeIRQDisable(u8 value)
{
    irqEnabled = false;
    // TODO: Acknowledge pending IRQ
}

// IRQ enable
void Mapper4::writeIRQEnable(u8 value)
{
    irqEnabled = true;
}

// Calculate PRG bank offset
s32 Mapper4::prgBankOffset(s32 index)
{
    if (index >= 0x80) {
        index -= 0x100;  // Handle negative indices
    }
    u32 prgSize = rom.GetHeader().prgRomBanks * PRGROM_BANK_SIZE;
    index %= prgSize / 0x2000;  // Number of 8KB banks
    s32 offset = index * 0x2000;
    if (offset < 0) {
        offset += prgSize;
    }
    return offset;
}

// Calculate CHR bank offset
s32 Mapper4::chrBankOffset(s32 index)
{
    if (index >= 0x80) {
        index -= 0x100;
    }
    u32 chrSize = rom.GetHeader().vRomBanks * VROM_BANK_SIZE;
    if (chrSize == 0) {
        return 0;  // CHR-RAM
    }
    index %= chrSize / 0x0400;  // Number of 1KB banks
    s32 offset = index * 0x0400;
    if (offset < 0) {
        offset += chrSize;
    }
    return offset;
}

// Update all bank offsets based on current configuration
void Mapper4::updateOffsets()
{
    // PRG banking modes:
    // Mode 0: $8000 swappable, $A000 swappable, $C000 fixed to -2, $E000 fixed to -1
    // Mode 1: $8000 fixed to -2, $A000 swappable, $C000 swappable, $E000 fixed to -1

    if (prgMode == 0) {
        prgOffsets[0] = prgBankOffset(bankRegisters[6]);
        prgOffsets[1] = prgBankOffset(bankRegisters[7]);
        prgOffsets[2] = prgBankOffset(-2);
        prgOffsets[3] = prgBankOffset(-1);
    } else {
        prgOffsets[0] = prgBankOffset(-2);
        prgOffsets[1] = prgBankOffset(bankRegisters[7]);
        prgOffsets[2] = prgBankOffset(bankRegisters[6]);
        prgOffsets[3] = prgBankOffset(-1);
    }

    // CHR banking modes:
    // Mode 0: 2KB banks at $0000, 2KB banks at $1000 (R0,R1 are 2KB, R2-R5 are 1KB)
    // Mode 1: 2KB banks at $1000, 2KB banks at $0000 (swapped)

    if (chrMode == 0) {
        // R0, R1 are 2KB banks (ignore low bit)
        chrOffsets[0] = chrBankOffset(bankRegisters[0] & 0xFE);
        chrOffsets[1] = chrBankOffset(bankRegisters[0] | 0x01);
        chrOffsets[2] = chrBankOffset(bankRegisters[1] & 0xFE);
        chrOffsets[3] = chrBankOffset(bankRegisters[1] | 0x01);
        // R2-R5 are 1KB banks
        chrOffsets[4] = chrBankOffset(bankRegisters[2]);
        chrOffsets[5] = chrBankOffset(bankRegisters[3]);
        chrOffsets[6] = chrBankOffset(bankRegisters[4]);
        chrOffsets[7] = chrBankOffset(bankRegisters[5]);
    } else {
        // Swapped: 2KB banks at $1000
        chrOffsets[0] = chrBankOffset(bankRegisters[2]);
        chrOffsets[1] = chrBankOffset(bankRegisters[3]);
        chrOffsets[2] = chrBankOffset(bankRegisters[4]);
        chrOffsets[3] = chrBankOffset(bankRegisters[5]);
        chrOffsets[4] = chrBankOffset(bankRegisters[0] & 0xFE);
        chrOffsets[5] = chrBankOffset(bankRegisters[0] | 0x01);
        chrOffsets[6] = chrBankOffset(bankRegisters[1] & 0xFE);
        chrOffsets[7] = chrBankOffset(bankRegisters[1] | 0x01);
    }
}

Mapper4::Mapper4(Rom& pRom)
    : rom(pRom)
    , bankSelect(0)
    , prgMode(0)
    , chrMode(0)
    , irqLatch(0)
    , irqCounter(0)
    , irqEnabled(false)
    , irqReload(false)
    , mirrorMode(0)
{
    // Initialize bank registers
    for (int i = 0; i < 8; i++) {
        bankRegisters[i] = 0;
    }

    // Set initial offsets
    updateOffsets();
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
