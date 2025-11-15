#pragma once

#include "rom.h"
#include "util.h"

namespace Frankenstein {

class Mapper {
public:
    enum MirrorMode {
        MirrorHorizontal = 0,
        MirrorVertical = 1,
        MirrorSingle0 = 2,
        MirrorSingle1 = 3,
        MirrorFour = 4
    };

    virtual u8 Read(u16 address) = 0;
    virtual void Write(u16 address, u8 value) = 0;
    virtual void Step() = 0;

    virtual ~Mapper() = 0;
};

class Mapper1 : public Mapper {
public:
    virtual u8 Read(u16 address) override;
    virtual void Write(u16 address, u8 value) override;
    virtual void Step() override;

    void loadRegister(u16 address, u8 value);
    void writeRegister(u16 address, u8 value);
    void writeControl(u8 value);
    void writeCHRBank0(u8 value);
    void writeCHRBank1(u8 value);
    void writePRGBank(u8 value);
    s32 prgBankOffset(s32 index);
    s32 chrBankOffset(s32 index);
    void updateOffsets();

    MirrorMode mirrorMode;

    Mapper1(Rom& pRom);
    ~Mapper1() override;

private:
    Rom& rom;
    u8 shiftRegister;
    u8 control;
    u8 prgMode;
    u8 chrMode;
    u8 prgBank;
    u8 chrBank0;
    u8 chrBank1;
    u32 prgOffsets[2];
    u32 chrOffsets[2];
};

class Mapper0 : public Mapper {
public:
    virtual u8 Read(u16 address) override;
    virtual void Write(u16 address, u8 value) override;
    virtual void Step() override;

    Mapper0(Rom& pRom);
    ~Mapper0() override;

private:
    Rom& rom;
    u8 prgBanks;  // 1 or 2 (16KB or 32KB)
};

class Mapper2 : public Mapper {
public:
    virtual u8 Read(u16 address) override;
    virtual void Write(u16 address, u8 value) override;
    virtual void Step() override;

    Mapper2(Rom& pRom);
    ~Mapper2() override;

private:
    Rom& rom;
    u8 prgBanks;
    u8 prgBank1;
    u8 prgBank2;
};

class Mapper3 : public Mapper {
public:
    virtual u8 Read(u16 address) override;
    virtual void Write(u16 address, u8 value) override;
    virtual void Step() override;

    Mapper3(Rom& pRom);
    ~Mapper3() override;

private:
    Rom& rom;
    u8 chrBank;      // Current CHR bank (0-3)
    u8 chrBanks;     // Total number of CHR banks
    u8 prgBanks;     // Number of PRG banks (1 or 2)
};

class Mapper4 : public Mapper {
public:
    virtual u8 Read(u16 address) override;
    virtual void Write(u16 address, u8 value) override;
    virtual void Step() override;

    Mapper4(Rom& pRom);
    ~Mapper4() override;

private:
    Rom& rom;

    // Bank select and configuration
    u8 bankSelect;       // Which bank register to update
    u8 bankRegisters[8]; // 8 bank registers (R0-R7)
    u8 prgMode;          // PRG banking mode (0 or 1)
    u8 chrMode;          // CHR banking mode (0 or 1)

    // Computed bank offsets for fast access
    u32 prgOffsets[4];   // Four 8KB PRG banks
    u32 chrOffsets[8];   // Eight 1KB CHR banks

    // IRQ counter
    u8 irqLatch;         // IRQ reload value
    u8 irqCounter;       // IRQ counter
    bool irqEnabled;     // IRQ enable flag
    bool irqReload;      // IRQ reload flag

    // Mirroring
    u8 mirrorMode;       // 0=vertical, 1=horizontal

    // Helper methods
    void updateOffsets();
    s32 prgBankOffset(s32 index);
    s32 chrBankOffset(s32 index);
    void writeBankSelect(u8 value);
    void writeBankData(u8 value);
    void writeMirror(u8 value);
    void writeProtect(u8 value);
    void writeIRQLatch(u8 value);
    void writeIRQReload(u8 value);
    void writeIRQDisable(u8 value);
    void writeIRQEnable(u8 value);
};

class Mapper7 : public Mapper {
public:
    virtual u8 Read(u16 address) override;
    virtual void Write(u16 address, u8 value) override;
    virtual void Step() override;

    Mapper7(Rom& pRom);
    ~Mapper7() override;

private:
    Rom& rom;
    u8 prgBank;      // Current 32KB PRG bank
    u8 prgBanks;     // Total number of 32KB banks
};
}
