# CLAUDE.md - AI Assistant Guide for Frankenstein NES Emulator

## Project Overview

**Frankenstein** is a bare metal NES (Nintendo Entertainment System) emulator for the Raspberry Pi 3 Model B, originally developed as an academic project at Sherbrooke University in Quebec, Canada. The project consists of approximately 6,000 lines of C++14 code and supports both Raspberry Pi bare metal execution and Linux desktop environments.

### Key Facts
- **Language**: C++14
- **Build System**: Meson 0.36.0+ with Ninja
- **Target Platforms**:
  - Raspberry Pi 3 Model B (bare metal, ARM)
  - Linux x86_64 (native with optional SFML graphics)
- **Version**: 0.1.0
- **Primary Authors**: David Michel Donais, Maxime Goyette

---

## Repository Structure

```
Frankenstein/
├── emulator/              # Core NES emulation engine (cross-platform)
│   ├── include/          # Public headers for all emulator components
│   ├── *.cpp             # Implementation files
│   ├── test/             # Unit tests with Google Test
│   └── meson.build       # Emulator build configuration
│
├── application/          # User-facing emulator frontends for Linux
│   ├── termEmulator.cpp  # Headless terminal emulator
│   ├── sfmlEmulator.cpp  # Graphical SFML-based emulator
│   └── meson.build       # Application build configuration
│
├── kernel/               # Raspberry Pi 3 bare metal kernel
│   ├── kernel.cpp        # Main kernel class with Circle integration
│   ├── main.cpp          # Entry point
│   ├── startup.S         # ARM assembly bootstrap
│   ├── config.txt        # RPi boot configuration
│   └── meson.build       # Kernel build configuration
│
├── subprojects/          # External dependencies
│   └── circle/           # Circle bare metal OS library (Git submodule)
│       ├── boot/         # Bootloader
│       ├── lib/          # Core OS functionality
│       ├── include/      # Hardware abstraction headers
│       └── addon/        # Additional features (UGUI, SDCard, USB gamepad)
│
├── nbproject/            # NetBeans IDE configuration files
├── meson.build           # Root build configuration
├── README.md             # User documentation
└── *.docx                # Detailed French documentation
```

---

## Core Components Deep Dive

### 1. CPU Emulation (`emulator/cpu.h`, `emulator/cpu.cpp`)

**Location**: `emulator/include/cpu.h:1-556`, `emulator/cpu.cpp:1-30000+`

**Purpose**: Emulates the MOS 6502 8-bit processor used in the NES.

**Key Implementation Details**:
- **Registers**: Accumulator (A), Index registers (X, Y), Program Counter (PC), Stack Pointer (SP), Status flags (P)
- **Status Flags**: Carry (C), Zero (Z), Interrupt Disable (I), Decimal (D), Break (B), Overflow (V), Sign (N)
- **Instruction Set**: 256 opcodes covering all 6502 instructions
- **Addressing Modes**: Immediate, Zero Page, Zero Page X/Y, Absolute, Absolute X/Y, Indirect, Indexed Indirect, Indirect Indexed
- **Interrupts**: NMI (Non-Maskable Interrupt) support for V-Blank synchronization

**Key Methods**:
- `Step()`: Execute one CPU instruction and return cycle count
- `NMI()`: Trigger non-maskable interrupt
- `setFlag()`, `getFlag()`: Status flag manipulation
- Instruction implementations: `LDA_*`, `STA_*`, `ADC_*`, etc. (one per addressing mode)

**Development Notes**:
- The CPU implementation is cycle-accurate for most instructions
- Each instruction variant has a dedicated method (e.g., `LDA_IMM`, `LDA_ABS`, `LDA_ABS_X`)
- Stack operations use page 0x01 (0x0100-0x01FF)
- When modifying: Ensure cycle counts remain accurate for timing-sensitive games

### 2. PPU (Picture Processing Unit) (`emulator/ppu.h`, `emulator/ppu.cpp`)

**Location**: `emulator/include/ppu.h:1-272`, `emulator/ppu.cpp:1-16000+`

**Purpose**: Emulates the RP2C02 graphics processor that generates video output.

**Key Features**:
- **Resolution**: 256x240 pixels (visible), 341x262 total scanlines per frame
- **Color Palette**: 64-color NTSC palette (defined in ppu.cpp)
- **Memory**:
  - Pattern tables: 8KB CHR-ROM for sprite/background tiles
  - Nametables: 2KB for background tile maps
  - OAM (Object Attribute Memory): 256 bytes for 64 sprites (4 bytes each)
  - Palette RAM: 32 bytes
- **Rendering Pipeline**:
  1. Visible scanlines (0-239): Render background and sprites
  2. Post-render scanline (240): Idle
  3. V-Blank scanlines (241-260): CPU can safely access VRAM
  4. Pre-render scanline (261): Prepare for next frame

**PPU Registers** (Memory-mapped at 0x2000-0x2007):
- `PPUCTRL` (0x2000): Control flags (nametable, increment, sprite/bg tables, NMI enable)
- `PPUMASK` (0x2001): Rendering flags (grayscale, clipping, enable bg/sprites, tint)
- `PPUSTATUS` (0x2002): Status flags (V-Blank, Sprite 0 hit, sprite overflow)
- `OAMADDR` (0x2003): OAM address pointer
- `OAMDATA` (0x2004): OAM data port
- `PPUSCROLL` (0x2005): Scroll position (write twice: X, then Y)
- `PPUADDR` (0x2006): VRAM address (write twice: high, then low byte)
- `PPUDATA` (0x2007): VRAM data port

**Development Notes**:
- PPU steps 3 times for each CPU cycle (3:1 ratio)
- V-Blank NMI is the primary synchronization mechanism with the CPU
- Sprite 0 hit detection is used for split-screen effects
- Mirroring modes (horizontal, vertical, single-screen, four-screen) affect nametable layout

### 3. Memory Management (`emulator/memory.h`, `emulator/memory_nes.cpp`)

**Location**: `emulator/include/memory.h:1-170`, `emulator/memory_nes.cpp:1-5000+`

**Purpose**: Manages the NES 64KB address space with memory-mapped I/O.

**NES Memory Map**:
```
0x0000-0x00FF: Zero Page (fast access)
0x0100-0x01FF: Stack
0x0200-0x07FF: General RAM (2KB)
0x0800-0x1FFF: Mirrors of 0x0000-0x07FF
0x2000-0x2007: PPU registers
0x2008-0x3FFF: Mirrors of PPU registers
0x4000-0x4015: APU registers
0x4016-0x4017: Controller ports
0x4020-0x5FFF: Expansion ROM
0x6000-0x7FFF: SRAM (battery-backed save RAM on cartridge)
0x8000-0xBFFF: PRG-ROM Lower Bank (16KB)
0xC000-0xFFFF: PRG-ROM Upper Bank (16KB)
```

**Template Design**: `Memory<DataType, AddressingType, Size>`
- Generic implementation allows different memory configurations
- Ref proxy pattern for transparent memory access
- Page crossing detection for cycle-accurate timing
- Supports all 6502 addressing modes

**Development Notes**:
- Memory reads/writes to PPU/APU registers have side effects
- Cartridge mappers can intercept and redirect memory access
- Write operations to ROM area are handled by mappers for bank switching

### 4. ROM Handling (`emulator/rom.h`, `emulator/rom_loader.h`, `emulator/rom_static.h`)

**Location**: `emulator/include/rom.h`, `emulator/rom.cpp`

**Purpose**: Parse iNES format ROM files and manage cartridge data.

**iNES Header Format** (16 bytes):
```
Byte 0-3:   Magic number "NES\x1A"
Byte 4:     Number of 16KB PRG-ROM banks
Byte 5:     Number of 8KB CHR-ROM banks
Byte 6:     Flags 6 (mapper low nibble, mirroring, battery, trainer, four-screen)
Byte 7:     Flags 7 (mapper high nibble, VS System, PlayChoice)
Byte 8:     Number of 8KB PRG-RAM banks
Byte 9:     TV system (NTSC/PAL)
Byte 10-15: Unused (padding)
```

**ROM Loading Modes**:
1. **Dynamic Loading** (`rom_loader.h`): Load .nes files from filesystem (Linux only)
2. **Static Embedding** (`rom_static.h`, `rom_static_data.cpp`): Compile ROM into binary (RPi3 bare metal)

**Development Notes**:
- For RPi3 builds, use `xxd -i romfile.nes > rom_static_data.cpp` to embed ROMs
- ROM parser extracts PRG-ROM, CHR-ROM, and SRAM data
- Trainer data (512 bytes) is supported but rarely used
- Mapper ID is constructed from flags 6 (low nibble) and flags 7 (high nibble)

### 5. Cartridge Mappers (`emulator/mapper.h`, `emulator/mapper.cpp`, `emulator/mapper_factory.h`)

**Location**: `emulator/include/mapper.h:1-104`, `emulator/mapper.cpp:1-6500+`

**Purpose**: Implement memory bank switching mechanisms used by different NES cartridges.

**Supported Mappers**:
- **Mapper 0 (NROM)**: No mapper, direct ROM access (32KB PRG, 8KB CHR)
- **Mapper 1 (SxROM/MMC1)**: Serial-based bank switching with 16KB PRG and 4KB CHR banks
- **Mapper 2 (UxROM)**: Switchable 16KB PRG bank + fixed bank (8KB CHR)
- **Mapper 3 (CNROM)**: Bank-switched CHR-ROM only
- **Mapper 4 (MMC3/TxROM)**: Advanced mapper with 8KB PRG banks, 2KB CHR banks, IRQ support
- **Mapper 7 (AxROM)**: 32KB PRG bank switching with single-screen mirroring

**Mapper Interface**:
```cpp
class Mapper {
    virtual uint8_t Read(uint16_t address) = 0;
    virtual void Write(uint16_t address, uint8_t value) = 0;
    virtual void Step() = 0;  // Called each PPU cycle
    virtual Mirror mirror() = 0;
};
```

**Development Notes**:
- Use `MapperFactory::getMapper(rom)` to instantiate correct mapper
- Mapper writes typically occur at 0x8000-0xFFFF (ROM address space)
- Some mappers (MMC3) have IRQ counters that require per-scanline `Step()` calls
- Adding new mappers: Extend `Mapper` base class and register in factory

### 6. Input/Gamepad (`emulator/gamepad.h`, `emulator/gamepad.cpp`)

**Location**: `emulator/include/gamepad.h:1-29`, `emulator/gamepad.cpp`

**Purpose**: Emulate NES controller input.

**Button Mapping** (8 buttons):
1. A
2. B
3. Select
4. Start
5. Up
6. Down
7. Left
8. Right

**Controller Protocol**:
1. CPU writes 1 to 0x4016 (strobe on)
2. CPU writes 0 to 0x4016 (strobe off) - latches button states
3. CPU reads 0x4016 eight times to get each button state (bit 0)
4. Each read shifts to next button

**Development Notes**:
- Two gamepads supported (0x4016 for pad1, 0x4017 for pad2)
- On RPi3, USB gamepad input is mapped in `kernel/kernel.cpp`
- Button states are updated during V-Blank (NMI) for smooth input

### 7. Main NES Class (`emulator/nes.h`, `emulator/nes.cpp`)

**Location**: `emulator/include/nes.h`, `emulator/nes.cpp`

**Purpose**: Orchestrate the entire emulation by coordinating CPU, PPU, memory, and input.

**Main Components**:
- `CPU cpu`: Processor instance
- `PPU ppu`: Graphics processor
- `Memory<...> ram`: Main memory
- `Gamepad pad1, pad2`: Controllers
- `Rom* rom`: Cartridge data reference
- `ScreenDevice* screen`: Display output interface

**Key Method**: `Step()`
```cpp
int NES::Step() {
    int cpuCycles = cpu.Step();
    for (int i = 0; i < cpuCycles * 3; i++) {
        ppu.Step();
    }
    return cpuCycles;
}
```

**Execution Flow**:
1. CPU executes one instruction (variable cycles)
2. PPU steps 3 times per CPU cycle
3. PPU generates NMI on V-Blank
4. CPU handles NMI, game logic updates
5. Repeat

**Development Notes**:
- Frame rate is ~60 Hz (based on NTSC timing: 262 scanlines, 341 cycles/scanline)
- Total cycles per frame: 262 * 341 = 89,342 PPU cycles = 29,780 CPU cycles
- Emulation speed depends on host CPU performance

---

## Build System (Meson + Ninja)

### Build Configurations

**Native Linux Build** (for development/testing):
```bash
mkdir test-build install-dir
meson test-build --buildtype release --prefix=$(pwd)/install-dir
cd test-build
ninja install
```

**Cross-Compile for Raspberry Pi 3**:
```bash
mkdir test-build install-dir
meson test-build --buildtype release \
  --prefix=$(pwd)/install-dir \
  --cross-file subprojects/circle/rpi3-cross.txt
cd test-build
ninja install
```

**Build Types**:
- `debug`: Debug symbols, no optimization
- `release`: Full optimization (-O3)
- `debugoptimized`: Debug symbols with optimization

### Build Outputs

**Native Build** (`install-dir/bin/`):
- `term-emulator`: Headless emulator for testing
- `sfml_emulator`: Graphical emulator (if SFML available)

**Cross Build** (`install-dir/kernel/`):
- `kernel7.img`: Bootable kernel image for RPi3 SD card
- Copy to SD card boot partition along with Circle bootloader files

### Cross-Compilation Toolchain

**File**: `subprojects/circle/rpi3-cross.txt`

**Required Tools**:
- `arm-none-eabi-gcc` (version 6-2017-q1 tested)
- `arm-none-eabi-g++`
- `arm-none-eabi-ar`
- `arm-none-eabi-ld`
- `arm-none-eabi-objcopy`

**Target**: ARMv8-A Cortex-A53 (32-bit mode)

**Key Flags**:
- `-march=armv8-a -mtune=cortex-a53`
- `-mfpu=neon-fp-armv8 -mfloat-abi=hard`
- `-ffreestanding -nostdlib -fno-exceptions -fno-rtti`
- `-D__circle__` (enables Circle-specific code paths)

### Dependencies

**Build-Time**:
- Meson ≥ 0.36.0
- Python ≥ 3.4
- Ninja ≥ 1.5
- C++14 compiler (GCC or Clang)
- Git (for submodule management)

**Runtime (Native)**:
- SFML ≥ 2.4 (optional, for graphics)
- pthreads

**Runtime (RPi3)**:
- Circle library (included as submodule)
- Raspberry Pi 3 hardware

**Testing**:
- Google Test 1.8.0 (auto-downloaded by Meson)

---

## Testing

### Unit Tests

**Location**: `emulator/test/`

**Test Files**:
- `cpu_test.cpp`: CPU instruction validation
- `memory_test.cpp`: Memory operations and addressing modes
- `common.h`: Shared test fixtures

**Test ROMs**: 17 diagnostic ROMs in `emulator/test/roms/`:
- `01-basics.nes` through `16-special.nes`: CPU instruction tests
- `color_test.nes`: PPU/graphics test

**Running Tests**:
```bash
cd test-build
ninja test
# Or directly:
./emulator/test/emulator_tests
```

**Test Fixtures**:
- `MemoryTest`: Loads test ROM and initializes NES
- `CPUTest`: Extends MemoryTest for CPU-specific tests
- `RomTest`: Extends CPUTest for ROM parsing tests

**Adding New Tests**:
1. Add test function in `*_test.cpp`: `TEST_F(CPUTest, TestName) { ... }`
2. Use Google Test assertions: `EXPECT_EQ()`, `ASSERT_TRUE()`, etc.
3. Run `ninja test` to execute

---

## Platform-Specific Code

### Linux Application Layer (`application/`)

**Terminal Emulator** (`termEmulator.cpp`):
- **Usage**: `./term-emulator path/to/rom.nes`
- **Features**: Headless execution, instruction logging, timing measurements
- **Output**: Debug trace to file

**SFML Emulator** (`sfmlEmulator.cpp`):
- **Usage**: `./sfml_emulator path/to/rom.nes`
- **Features**: 256x240 window, multi-threaded rendering, V-Blank sync
- **Controls**: Currently ROM-based (hardcoded in current version)
- **Threading**: Emulator runs in separate thread, main thread handles rendering

**Development Notes**:
- Both emulators use `rom_loader.h` for dynamic ROM loading
- Frame rate regulation in SFML version uses V-Blank detection
- To add input handling: Modify SFML event loop to call `pad1.setButtonState()`

### Raspberry Pi Bare Metal (`kernel/`)

**Kernel Class** (`kernel.cpp`):
- **Circle Integration**: Initializes all Circle subsystems
- **Display**: 512x480 HDMI output via `CScreenDevice`
- **Serial**: 115200 baud UART logging
- **USB**: USB gamepad support via `CDWHCIDevice`
- **Input Mapping**: USB gamepad buttons mapped to NES controller in `UpdateGamepad()`

**Main Loop**:
```cpp
while (true) {
    nes.Step();
    if (NMI occurred) {
        UpdateGamepad(pad1, usbGamepad);
        EnableUSBIRQ();
    }
}
```

**Boot Process**:
1. ARM core starts, executes `startup.S`
2. Calls `main()` in `main.cpp`
3. Creates `CKernel` instance
4. Calls `kernel.Initialize()` - sets up hardware
5. Calls `kernel.Run()` - enters emulation loop
6. On exit, returns shutdown mode to bootloader

**Configuration** (`config.txt`):
- HDMI mode: 512x480@60Hz
- Force turbo mode for performance
- Copied to SD card boot partition

**Deployment**:
1. Build with cross-compilation
2. Copy `kernel7.img` to SD card boot partition
3. Copy Circle bootloader files (`bootcode.bin`, `start.elf`, `fixup.dat`)
4. Copy `config.txt`
5. Insert SD card and power on RPi3

---

## Development Workflow

### Adding New Features

**1. CPU Instructions**:
- **File**: `emulator/cpu.cpp`, `emulator/include/cpu.h`
- **Steps**:
  1. Add instruction method in cpu.cpp: `void CPU::INSTR_MODE() { ... }`
  2. Add declaration in cpu.h
  3. Add to instruction table in cpu.cpp
  4. Set correct cycle count
  5. Write unit test in `emulator/test/cpu_test.cpp`
  6. Run tests: `ninja test`

**2. PPU Features**:
- **File**: `emulator/ppu.cpp`, `emulator/include/ppu.h`
- **Steps**:
  1. Identify which rendering stage needs modification
  2. Update rendering pipeline in `ppu.cpp`
  3. Test with `color_test.nes` or similar graphics ROM
  4. Verify in SFML emulator visually

**3. New Mapper Support**:
- **Files**: `emulator/mapper.cpp`, `emulator/mapper_factory.cpp`
- **Steps**:
  1. Create new mapper class extending `Mapper` base class
  2. Implement `Read()`, `Write()`, `Step()`, `mirror()` methods
  3. Add to `MapperFactory::getMapper()` switch statement
  4. Test with ROMs that use the mapper
  5. Update this document with mapper details

**4. Input Features**:
- **Linux**: Modify `application/sfmlEmulator.cpp` event handling
- **RPi3**: Modify `kernel/kernel.cpp` `UpdateGamepad()` function
- Map new inputs to `Gamepad::setButtonState(button, state)`

### Code Style Conventions

**Naming**:
- Classes: PascalCase (`class CPU`, `class PPU`)
- Methods: camelCase (`void setFlag()`, `uint8_t Read()`)
- Members: camelCase with context-appropriate prefixes
- Constants: UPPER_SNAKE_CASE (`const int MAX_CYCLES`)

**File Organization**:
- Headers in `include/` directory
- Implementations in root of component directory
- One class per file pair (header + source)
- Use include guards: `#ifndef FILE_H` / `#define FILE_H` / `#endif`

**Comments**:
- Use `//` for single-line comments
- Use `/* */` for multi-line explanations
- Document complex algorithms with reference to NES hardware behavior
- Include URLs to NESDev wiki for hardware-specific implementations

**Memory Management**:
- Prefer stack allocation over heap
- Use RAII principles
- For RPi3: Avoid dynamic allocation in main loop (no `new`/`delete`)
- Circle library provides placement new if needed

### Debugging Strategies

**CPU Debugging**:
1. Enable instruction logging in `termEmulator.cpp`
2. Compare trace with expected output from test ROM
3. Check flags after arithmetic operations
4. Verify cycle counts match expected timing
5. Use test ROMs (`01-basics.nes` etc.) to isolate issues

**PPU Debugging**:
1. Use `color_test.nes` for palette verification
2. Log scanline/cycle positions for rendering bugs
3. Check V-Blank timing (should occur at scanline 241)
4. Verify mirroring mode matches ROM requirements
5. Check sprite 0 hit detection for split-screen effects

**Memory Issues**:
1. Verify mapper is correctly intercepting reads/writes
2. Check memory mirroring (0x0800-0x1FFF mirrors 0x0000-0x07FF)
3. Ensure PPU register reads have side effects (status flag clearing)
4. Validate cartridge SRAM boundaries (0x6000-0x7FFF)

**RPi3-Specific**:
1. Use serial output for logging (`CLogger`)
2. Blink LED for status indication (`CActLED`)
3. Check USB gamepad detection in kernel initialization
4. Verify HDMI output mode in `config.txt`
5. Test on Linux first before deploying to RPi3

---

## Key Conventions for AI Assistants

### Code Modification Guidelines

**DO**:
- ✅ Read existing code thoroughly before making changes
- ✅ Maintain cycle-accurate timing when modifying CPU/PPU
- ✅ Test changes with unit tests and test ROMs
- ✅ Follow existing code style and naming conventions
- ✅ Add comments explaining NES-specific hardware behavior
- ✅ Update this CLAUDE.md when adding significant features
- ✅ Use git for version control with descriptive commit messages
- ✅ Preserve cross-platform compatibility (Linux and RPi3)
- ✅ Check for memory safety (no buffer overflows)
- ✅ Verify changes work in both native and cross-compiled builds

**DON'T**:
- ❌ Break cycle accuracy without documenting trade-offs
- ❌ Add features that require C++ standard library on RPi3 (use Circle equivalents)
- ❌ Introduce dynamic allocation in hot paths (main emulation loop)
- ❌ Change memory map without updating documentation
- ❌ Add dependencies without verifying cross-compilation support
- ❌ Modify Circle library files directly (patch if absolutely necessary)
- ❌ Remove test coverage
- ❌ Hardcode ROM paths or data (use loader for Linux, static for RPi3)

### Understanding Context

**When Asked About**:

**"CPU isn't working"**:
1. Check which instruction is failing (`termEmulator` logging)
2. Verify addressing mode implementation
3. Check flag manipulation (especially for branch instructions)
4. Confirm cycle count accuracy
5. Review stack operations if crashes occur

**"Graphics are wrong"**:
1. Identify if background, sprites, or both are affected
2. Check PPU register writes (PPUCTRL, PPUMASK, PPUSCROLL, PPUADDR)
3. Verify pattern table and nametable addresses
4. Check palette RAM correctness
5. Verify mirroring mode matches cartridge
6. Test with `color_test.nes`

**"Game doesn't load"**:
1. Verify ROM file format (iNES header validation)
2. Check if mapper is supported (see mapper list above)
3. Ensure PRG-ROM and CHR-ROM sizes are correct
4. Validate cartridge mirroring mode
5. Check if ROM is corrupted (compare checksum)

**"Input doesn't work"**:
1. Verify gamepad strobe sequence in game code
2. Check button state updates timing (should be during V-Blank)
3. For RPi3: Verify USB gamepad detection in kernel logs
4. Test controller port reads (0x4016/0x4017)

**"Performance is slow"**:
1. Check build type (should be `release` for full optimization)
2. Verify timing loop isn't adding unnecessary delays
3. For RPi3: Ensure turbo mode is enabled in `config.txt`
4. Profile hot paths (CPU instruction dispatch, PPU rendering)
5. Consider optimizing mapper implementations

### Common Tasks Reference

**Adding a Test ROM**:
```bash
# 1. Copy ROM to test directory
cp new_test.nes emulator/test/roms/

# 2. Add test in cpu_test.cpp or memory_test.cpp
TEST_F(CPUTest, NewTestROM) {
    // Load and run test
}

# 3. Rebuild and run tests
ninja -C test-build test
```

**Embedding a ROM for RPi3**:
```bash
# 1. Convert ROM to C array
xxd -i game.nes > emulator/rom_static_data.cpp

# 2. Update emulator/include/rom_static.h with new array name

# 3. Rebuild with cross-compilation
ninja -C test-build install
```

**Updating Circle Library**:
```bash
# 1. Navigate to subproject
cd subprojects/circle

# 2. Update to latest version
git fetch origin
git checkout <version-tag>

# 3. Rebuild
cd ../..
rm -rf test-build
meson test-build --cross-file subprojects/circle/rpi3-cross.txt
ninja -C test-build
```

**Adding New Compiler Flags**:
```meson
# Edit meson.build
if get_option('buildtype') == 'debug'
    add_project_arguments('-DDEBUG_MODE', language: 'cpp')
endif
```

---

## External Resources

### NES Development References
- **NESDev Wiki**: https://wiki.nesdev.com/
  - CPU: https://wiki.nesdev.com/w/index.php/CPU
  - PPU: https://wiki.nesdev.com/w/index.php/PPU
  - Mappers: https://wiki.nesdev.com/w/index.php/Mapper
  - Memory Map: https://wiki.nesdev.com/w/index.php/CPU_memory_map

### Circle Library
- **GitHub**: https://github.com/rsta2/circle
- **Documentation**: https://github.com/rsta2/circle/tree/master/doc
- **Samples**: https://github.com/rsta2/circle/tree/master/sample

### Build System
- **Meson**: https://mesonbuild.com/
- **Ninja**: https://ninja-build.org/

### Testing
- **Google Test**: https://github.com/google/googletest

---

## Project History & Credits

**Original Authors**:
- David Michel Donais (david.michel.donais@usherbrooke.ca)
- Maxime Goyette (maxime.g.goyette@usherbrooke.ca)

**Institution**: Université de Sherbrooke, Quebec, Canada

**Acknowledgments**:
- Circle library by rsta2: https://github.com/rsta2/circle
- PPU implementation ported from: https://github.com/fogleman/nes
- All other code is original work by the authors

**License**: See individual file headers and Circle library license

---

## Troubleshooting

### Build Issues

**"Meson version too old"**:
```bash
pip3 install --user --upgrade meson
```

**"arm-none-eabi-gcc not found"**:
```bash
# Ubuntu/Debian:
sudo apt-get install gcc-arm-none-eabi

# Or download from ARM:
# https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
```

**"SFML not found"**:
```bash
# Ubuntu/Debian:
sudo apt-get install libsfml-dev

# Or disable SFML build:
# Meson will automatically skip SFML emulator if library not found
```

**"Google Test download fails"**:
- Check internet connection
- Meson will auto-download GTest on first build
- Alternatively, install system package: `sudo apt-get install libgtest-dev`

### Runtime Issues

**"kernel7.img doesn't boot on RPi3"**:
1. Verify SD card has Circle bootloader files
2. Check `config.txt` is present and correctly formatted
3. Ensure kernel7.img is in boot partition root
4. Try serial console output to see boot messages
5. Verify cross-compilation used correct toolchain

**"Emulator crashes immediately"**:
1. Verify ROM file is valid iNES format
2. Check if mapper is supported
3. Run with debugger: `gdb ./term-emulator`
4. Check for memory access violations in mapper

**"Graphics are corrupted"**:
1. Verify CHR-ROM data loaded correctly
2. Check palette initialization
3. Test with known-good ROM (color_test.nes)
4. Verify mirroring mode

**"Input lag or missed inputs"**:
1. Ensure input update happens during V-Blank
2. Check gamepad strobe timing
3. Verify USB polling rate on RPi3
4. Test with simpler input sequences

---

## Future Development Ideas

**Potential Enhancements**:
- [ ] Audio Processing Unit (APU) emulation for sound
- [ ] Additional mapper support (5, 9, 10, etc.)
- [ ] Save state functionality
- [ ] Rewind feature
- [ ] Cheat code support
- [ ] Network multiplayer
- [ ] Game Genie emulation
- [ ] Better input configuration (key mapping)
- [ ] Performance profiling tools
- [ ] Debugger interface (step through, breakpoints)
- [ ] PAL (European NES) support

**Architecture Improvements**:
- [ ] Plugin system for mappers
- [ ] Modular APU for easier sound implementation
- [ ] Abstracted screen interface for different display backends
- [ ] Save file management
- [ ] Configuration file support

---

## Quick Reference

### Important File Locations

| Component | Header | Implementation | Lines |
|-----------|--------|----------------|-------|
| CPU | `emulator/include/cpu.h` | `emulator/cpu.cpp` | 30,000+ |
| PPU | `emulator/include/ppu.h` | `emulator/ppu.cpp` | 16,000+ |
| Memory | `emulator/include/memory.h` | `emulator/memory_nes.cpp` | 5,000+ |
| Mappers | `emulator/include/mapper.h` | `emulator/mapper.cpp` | 6,500+ |
| ROM Parser | `emulator/include/rom.h` | `emulator/rom.cpp` | - |
| Gamepad | `emulator/include/gamepad.h` | `emulator/gamepad.cpp` | - |
| NES Core | `emulator/include/nes.h` | `emulator/nes.cpp` | - |
| RPi3 Kernel | `kernel/kernel.h` | `kernel/kernel.cpp` | 150+ |

### Build Commands Quick Reference

```bash
# Native Linux build
meson build --buildtype=release
ninja -C build
ninja -C build test

# Cross-compile for RPi3
meson build-rpi --cross-file subprojects/circle/rpi3-cross.txt
ninja -C build-rpi

# Clean build
rm -rf build
meson build
ninja -C build

# Run emulator
./build/application/sfml_emulator path/to/game.nes
./build/application/term-emulator path/to/test.nes

# Deploy to RPi3
cp build-rpi/kernel/kernel7.img /path/to/sd_card/
```

---

## Document Maintenance

**Last Updated**: 2025-11-15

**Maintained By**: AI Assistant (Claude)

**Update Frequency**: Update this document when:
- New major features are added
- Architecture changes significantly
- Build system is modified
- New dependencies are added
- Development workflows change

**How to Update**:
1. Edit this CLAUDE.md file
2. Update "Last Updated" date
3. Commit with message: `docs: Update CLAUDE.md with [description]`
4. Keep information accurate and concise for AI assistant reference

---

*This document is designed to help AI assistants understand and work with the Frankenstein NES emulator codebase effectively. For user-facing documentation, see README.md and the French documentation files.*
