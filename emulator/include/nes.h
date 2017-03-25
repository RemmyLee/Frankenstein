#ifndef NES_H
#define NES_H

#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "memory.h"
#include "gamepad.h"

namespace Frankenstein {

class Nes
{
public:
    Gamepad pad1;
    Gamepad pad2;
    NesMemory ram;
    const IRom &rom;
    Cpu cpu;
    Ppu ppu;

    Nes(IRom &rom);
    void Step();
};

}

#endif // CPU_H
