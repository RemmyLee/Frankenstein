# Frankenstein NES Emulator - Multi-ROM Implementation Plan

## Executive Summary

This document outlines the comprehensive plan to transform the Frankenstein NES emulator from a single-ROM bare metal application into a full-featured multi-game emulator with ROM browser, extensive mapper support, and performance optimizations.

## Current State Analysis

### Critical Issues Identified

1. **BROKEN MAPPERS** ⚠️
   - **Mapper 3, 4, 7 are NOT implemented** - They return 0 for all reads/writes
   - Only Mappers 1 and 2 are functional
   - Mapper 0 incorrectly uses Mapper 2 implementation (see `mapper_factory.cpp:9`)
   - This means **most games won't work at all**

2. **Static ROM Loading**
   - ROM is embedded at compile time (`rom_static_data.cpp`)
   - Requires recompilation to change games
   - No runtime ROM loading capability

3. **No File System Integration**
   - Circle library has SD card support available but unused
   - No FAT filesystem integration
   - Cannot read ROMs from SD card

4. **No User Interface**
   - Direct boot into game
   - No ROM selection menu
   - No configuration options

5. **Performance Concerns**
   - Unknown if current speed is adequate for all games
   - No benchmarking or profiling done
   - Potential bottlenecks in CPU/PPU rendering loops

### What Works

✅ Raspberry Pi 3 bare metal execution
✅ HDMI output (512x480)
✅ USB gamepad input
✅ NES CPU emulation (6502)
✅ NES PPU emulation (graphics)
✅ Mapper 1 (MMC1/SxROM) - fully functional
✅ Mapper 2 (UxROM) - fully functional
✅ Build system (Meson + Ninja)

---

## Implementation Roadmap

### PHASE 1: Mapper Research & Implementation (Critical Priority)

**Goal**: Implement all commonly-used NES mappers to support 95%+ of games

#### 1.1 Mapper Coverage Analysis

Research required mappers based on game library coverage:

**Top Priority Mappers** (covers ~90% of games):
- **Mapper 0 (NROM)** - Fix current implementation (currently uses Mapper 2)
- **Mapper 1 (MMC1/SxROM)** - ✅ Already implemented
- **Mapper 2 (UxROM)** - ✅ Already implemented
- **Mapper 3 (CNROM)** - 🔴 BROKEN - Implement fully
- **Mapper 4 (MMC3/TxROM)** - 🔴 BROKEN - Most complex, highest priority
- **Mapper 7 (AxROM)** - 🔴 BROKEN - Implement fully
- **Mapper 9 (MMC2)** - NEW - Punch-Out!!
- **Mapper 10 (MMC4)** - NEW - Fire Emblem
- **Mapper 11 (Color Dreams)** - NEW - Several unlicensed games

**Secondary Priority Mappers** (covers additional 5%):
- **Mapper 5 (MMC5)** - Castlevania 3, Metal Slayer
- **Mapper 66 (GxROM)** - Several games
- **Mapper 71** - Camerica games
- **Mapper 87** - Rare games

#### 1.2 Mapper Implementation Details

**Mapper 0 (NROM)** - 32KB PRG, 8KB CHR, no banking
```cpp
// CRITICAL FIX: mapper_factory.cpp line 8-9
// Currently: case 0: return new Mapper2(rom);
// Should be: case 0: return new Mapper0(rom);

class Mapper0 : public Mapper {
    // Simple pass-through, no banking
    // PRG-ROM: 0x8000-0xFFFF (16KB or 32KB)
    // CHR-ROM: 0x0000-0x1FFF (8KB)
    // SRAM: 0x6000-0x7FFF (8KB)
};
```

**Mapper 3 (CNROM)** - Bank-switched CHR-ROM
```cpp
class Mapper3 : public Mapper {
    // PRG-ROM: Fixed 16KB or 32KB
    // CHR-ROM: Switchable 8KB banks (up to 4 banks)
    // Write to 0x8000-0xFFFF selects CHR bank
private:
    u8 chrBank;
    u8 chrBanks;
};
```

**Mapper 4 (MMC3)** - MOST IMPORTANT, most complex
```cpp
class Mapper4 : public Mapper {
    // Bank switching for PRG and CHR
    // IRQ counter for scanline detection
    // Critical for: Super Mario Bros 2/3, Mega Man 3-6, etc.

    // Registers:
    // 0x8000-0x9FFF: Bank select + data
    // 0xA000-0xBFFF: Mirroring + PRG RAM protect
    // 0xC000-0xDFFF: IRQ latch + reload
    // 0xE000-0xFFFF: IRQ disable + enable

private:
    u8 bankSelect;
    u8 bankRegisters[8];
    u8 prgMode;
    u8 chrMode;
    u8 irqLatch;
    u8 irqCounter;
    bool irqEnabled;
    bool irqReload;
    u32 prgOffsets[4];
    u32 chrOffsets[8];

    void updateOffsets();
    void handleIRQ(); // Called each scanline
};
```

**Mapper 7 (AxROM)** - 32KB switchable PRG banks
```cpp
class Mapper7 : public Mapper {
    // PRG-ROM: Switchable 32KB banks
    // CHR-RAM: 8KB (not ROM)
    // Single-screen mirroring controlled by bit 4

private:
    u8 prgBank;
    u8 prgBanks;
};
```

#### 1.3 Mapper Testing Strategy

For each mapper:
1. Implement based on NESDev wiki specifications
2. Test with known-good ROMs:
   - Mapper 0: Donkey Kong, Balloon Fight, Ice Climber
   - Mapper 1: Mega Man 2, Metroid, Legend of Zelda
   - Mapper 2: Mega Man, Castlevania
   - Mapper 3: Q*Bert, Gradius
   - Mapper 4: Super Mario Bros 2/3, Kirby's Adventure
   - Mapper 7: Battletoads, Marble Madness
3. Compare behavior with reference emulator (FCEUX, Nintendulator)
4. Validate with test ROMs from NESDev

**Estimated Time**: 3-4 weeks

---

### PHASE 2: File System & SD Card Integration

**Goal**: Enable reading ROMs from SD card FAT32 filesystem

#### 2.1 Circle FAT Filesystem Research

Circle library components needed:
- `CEMMCDevice` - SD card hardware interface (from addon/SDCard)
- FatFs library integration (may need to add)
- Partition manager
- File I/O abstraction

**Research Tasks**:
1. Investigate Circle's file system capabilities
2. Check if FatFs (Chan's FAT library) is available
3. Determine if we need to integrate external FatFs
4. Create file system abstraction layer

#### 2.2 SD Card Initialization

Add to `kernel.cpp`:
```cpp
#include <addon/SDCard/emmc.h>

class CKernel {
private:
    CEMMCDevice m_EMMC;
    // CFileSystem or CFatFileSystem

boolean CKernel::Initialize(void) {
    // ... existing initialization ...

    if (bOK) {
        bOK = m_EMMC.Initialize();
        if (!bOK) {
            m_Logger.Write(FromKernel, LogError, "SD Card init failed");
        }
    }

    // Mount FAT filesystem
    // Enumerate ROMs directory
}
};
```

#### 2.3 ROM Loading Architecture

**Directory Structure** (on SD card):
```
/ROMS/
  ├── adventure/
  │   ├── zelda.nes
  │   ├── metroid.nes
  │   └── ...
  ├── action/
  │   ├── mario.nes
  │   ├── megaman.nes
  │   └── ...
  └── ...
```

**ROM Loader Class**:
```cpp
class RomFileLoader {
public:
    struct RomEntry {
        char filename[256];
        char path[512];
        u32 fileSize;
        u8 mapper;
        char gameName[128]; // Extracted from ROM or filename
    };

    boolean ScanDirectory(const char* path);
    std::vector<RomEntry> GetRomList();
    Rom* LoadRom(const char* path);
    void FreeRom(Rom* rom);

private:
    std::vector<RomEntry> romList;
};
```

**Memory Management Considerations**:
- ROMs can be 40KB - 512KB+
- RPi3 has 1GB RAM but bare metal - be careful with allocations
- Consider memory pool for ROM data
- Free previous ROM before loading new one

#### 2.4 iNES Header Parsing Enhancement

Enhance ROM parser to extract game info:
```cpp
struct RomMetadata {
    char title[128];        // From header or database
    u8 mapper;
    u8 prgBanks;
    u8 chrBanks;
    bool battery;           // Has save RAM
    MirrorMode mirroring;
};
```

**Estimated Time**: 2-3 weeks

---

### PHASE 3: ROM Browser & Menu System

**Goal**: Create intuitive ROM selection interface

#### 3.1 UI Framework Selection

**Option A: Text-based Menu** (Simpler, faster to implement)
- Render text directly to framebuffer
- List ROMs vertically
- D-pad navigation, A to select
- Shows: ROM name, mapper, file size

**Option B: UGUI-based Menu** (Better UX, more work)
- Use Circle's UGUI addon
- Graphical elements, icons
- Thumbnails (if we add screenshot support)
- More professional appearance

**Recommendation**: Start with Option A for MVP, upgrade to Option B later

#### 3.2 Text-Based Menu Implementation

**Menu States**:
```cpp
enum MenuState {
    STATE_ROM_LIST,      // Browsing ROM list
    STATE_ROM_INFO,      // Viewing ROM details
    STATE_IN_GAME,       // Playing game
    STATE_SETTINGS       // Future: settings menu
};
```

**Menu Renderer**:
```cpp
class MenuRenderer {
public:
    void RenderRomList(std::vector<RomEntry>& roms, int selectedIndex);
    void RenderRomInfo(RomEntry& rom);
    void RenderLoadingScreen();

    void DrawText(int x, int y, const char* text, u32 color);
    void DrawBox(int x, int y, int w, int h, u32 color);
    void Clear(u32 color);

private:
    CScreenDevice* screen;
    // Font rendering
};
```

**Input Handling**:
```cpp
class MenuInputHandler {
public:
    void Update(TGamePadState& gamepad);

    bool IsUpPressed();
    bool IsDownPressed();
    bool IsAPressed();      // Select
    bool IsBPressed();      // Back
    bool IsStartPressed();  // Start game

private:
    TGamePadState previousState;
    TGamePadState currentState;
};
```

#### 3.3 Menu Flow

```
[Boot]
  ↓
[Initialize SD Card]
  ↓
[Scan for ROMs] → (if none found) → [Error: No ROMs]
  ↓
[ROM List Menu]
  ├─ Up/Down: Navigate
  ├─ A: View details / Start
  ├─ B: Back (if in submenu)
  └─ Start: Quick launch
  ↓
[Load ROM]
  ↓
[Initialize Emulator]
  ↓
[Game Running]
  ├─ Select+Start: Return to menu
  └─ (game plays normally)
```

#### 3.4 Font Rendering

Need basic font for text:
- Use Circle's built-in character generator
- Or include small bitmap font (8x8 or 8x16)
- Support ASCII characters for filenames

**Estimated Time**: 2-3 weeks

---

### PHASE 4: Performance Optimization

**Goal**: Achieve smooth 60 FPS emulation for all games

#### 4.1 Current Performance Baseline

**Profiling Tasks**:
1. Measure current frame rate
2. Identify bottlenecks with timing code
3. Profile CPU instruction execution time
4. Profile PPU rendering time
5. Measure memory access patterns

**Add Profiling Code**:
```cpp
class Profiler {
public:
    void StartFrame();
    void EndFrame();
    void ReportCPUTime(u64 cycles);
    void ReportPPUTime(u64 cycles);
    void PrintStats(); // Every 60 frames

private:
    u64 frameStartTime;
    u64 totalCPUCycles;
    u64 totalPPUCycles;
    u32 frameCount;
};
```

#### 4.2 CPU Optimizations

**Hot Path Analysis**:
- Instruction dispatch (switch/case or function pointer table)
- Memory access (reduce indirection)
- Flag calculations

**Optimization Strategies**:

1. **Inline Critical Functions**:
```cpp
inline void Cpu::setFlag(Flag flag, bool value) {
    // Force inline for flag operations
}
```

2. **Optimize Instruction Dispatch**:
```cpp
// Consider jump table instead of switch
typedef int (Cpu::*InstructionHandler)();
static const InstructionHandler instructionTable[256] = { ... };

void Cpu::Step() {
    u8 opcode = OpCode();
    cycles = (this->*instructionTable[opcode])();
}
```

3. **Reduce Memory Indirection**:
```cpp
// Cache frequently accessed values
u8* ramPtr = &nes.ram[0];
// Direct access instead of operator[]
```

4. **Compiler Optimization Flags**:
```meson
# meson.build for RPi3
cpp_args = [
    '-O3',                    # Maximum optimization
    '-march=armv8-a',        # Target architecture
    '-mtune=cortex-a53',     # CPU tuning
    '-mfpu=neon-fp-armv8',   # NEON SIMD
    '-ffast-math',           # Faster math (if safe)
    '-funroll-loops',        # Loop unrolling
    '-finline-functions',    # Aggressive inlining
]
```

#### 4.3 PPU Optimizations

**Rendering Bottlenecks**:
- Pixel-by-pixel rendering
- Palette lookups
- Sprite evaluation
- Background tile fetching

**Optimization Strategies**:

1. **NEON SIMD for Palette Conversion**:
```cpp
// Use ARM NEON intrinsics for parallel color conversion
#include <arm_neon.h>

void ConvertPaletteSIMD(u8* paletteIndices, RGBColor* output, int count) {
    // Process 8 pixels at once with NEON
}
```

2. **Scanline-based Rendering** (Already doing this?):
- Render entire scanline at once
- Batch framebuffer writes

3. **Dirty Rectangle Tracking**:
- Only update changed portions of screen
- Reduce framebuffer writes

4. **Pre-computed Lookups**:
```cpp
// Pre-compute mirror address lookup tables
static const u16 mirrorLUT[4][0x1000];

// Pre-compute palette RGB values
static const RGBColor paletteRGB[64];
```

#### 4.4 Memory Optimizations

1. **Alignment**:
```cpp
// Align critical data structures
struct alignas(32) PpuRegisters {
    // 32-byte alignment for cache line
};
```

2. **Data Locality**:
- Keep hot data in contiguous memory
- Reduce cache misses

3. **Reduce Dynamic Allocation**:
```cpp
// Pre-allocate ROM buffer pool
static u8 romBuffer[MAX_ROM_SIZE] __attribute__((aligned(32)));
```

#### 4.5 Target Performance Metrics

**Goals**:
- Maintain 60 FPS (16.67ms per frame)
- CPU emulation: < 10ms per frame
- PPU rendering: < 6ms per frame
- Headroom for menu/UI: 1-2ms

**Estimated Time**: 3-4 weeks

---

### PHASE 5: Testing & Validation

**Goal**: Ensure compatibility and stability across diverse ROM library

#### 5.1 Test ROM Library

**Mapper-Specific Test Suite**:
- Download comprehensive test ROM collection from NESDev
- Test ROMs for each mapper
- CPU instruction test ROMs
- PPU rendering test ROMs

**Real Game Testing**:

| Mapper | Games to Test |
|--------|---------------|
| 0 | Donkey Kong, Balloon Fight, Ice Climber, Excitebike |
| 1 | Zelda 1, Metroid, Mega Man 2, Bomberman 2 |
| 2 | Mega Man 1, Castlevania 1, DuckTales |
| 3 | Q*Bert, Gradius, Cybernoid |
| 4 | SMB2, SMB3, Kirby's Adventure, Mega Man 3-6 |
| 7 | Battletoads, Wizards & Warriors, Marble Madness |

#### 5.2 Automated Testing

```cpp
class EmulatorTestRunner {
public:
    void RunCPUTests();
    void RunPPUTests();
    void RunMapperTests(u8 mapper);
    void RunCompatibilityTest(const char* romPath);

    void LogResults();

private:
    struct TestResult {
        const char* testName;
        bool passed;
        const char* errorMsg;
    };

    std::vector<TestResult> results;
};
```

#### 5.3 Regression Testing

- Create test suite for each milestone
- Run full suite before each release
- Track compatibility percentage

#### 5.4 Performance Testing

```cpp
class PerformanceBenchmark {
public:
    void BenchmarkROM(const char* romPath, u32 frameCount);
    void ReportFrameRate();
    void ReportFrameTime();
    void ReportDroppedFrames();
};
```

**Estimated Time**: 2-3 weeks (ongoing)

---

### PHASE 6: Polish & Additional Features

**Goal**: User experience improvements and nice-to-have features

#### 6.1 Save State Support

```cpp
class SaveStateManager {
public:
    bool SaveState(const char* romPath, u8 slot);
    bool LoadState(const char* romPath, u8 slot);

private:
    struct SaveState {
        // CPU state
        CpuRegisters cpu;
        u8 ram[0x800];

        // PPU state
        PpuRegisters ppu;
        u8 vram[0x800];
        u8 oam[256];
        u8 palette[32];

        // Mapper state
        u8 mapperData[256];
    };
};
```

#### 6.2 Configuration System

```cpp
struct EmulatorConfig {
    u8 audioVolume;         // 0-100
    bool showFPS;
    u8 screenScaleMode;     // Aspect ratio options
    u8 colorPalette;        // NTSC/PAL/Custom
    // Controller mapping
};
```

#### 6.3 On-Screen Display

- FPS counter
- ROM name
- Current mapper
- Battery indicator (if low)

#### 6.4 Audio (APU) Implementation

**Major Feature** - Not in critical path but highly desired:
- Implement NES APU (Audio Processing Unit)
- 5 audio channels: 2x Pulse, Triangle, Noise, DMC
- Circle has audio output support
- Significant complexity - separate phase

**Estimated Time**: 3-4 weeks

---

## Implementation Timeline

### Sprint Structure (2-week sprints)

**Sprint 1-2: Critical Mappers**
- Fix Mapper 0
- Implement Mapper 3
- Implement Mapper 7
- Begin Mapper 4 implementation

**Sprint 3-4: Mapper 4 & Testing**
- Complete Mapper 4 (most complex)
- Test all mappers with real ROMs
- Bug fixes

**Sprint 5-6: File System**
- Research Circle FS capabilities
- Integrate SD card support
- Implement ROM scanning
- Dynamic ROM loading

**Sprint 7-8: Menu System**
- Text-based menu implementation
- ROM list rendering
- Input handling
- Integration with ROM loader

**Sprint 9-10: Performance**
- Profiling and benchmarking
- CPU optimizations
- PPU optimizations
- NEON SIMD implementation

**Sprint 11-12: Testing & Polish**
- Extensive compatibility testing
- Bug fixes
- Performance tuning
- Documentation updates

**Total Estimated Time**: 24 weeks (6 months)

---

## Technical Risks & Mitigation

### Risk 1: Mapper Complexity
**Risk**: Mapper 4 (MMC3) IRQ implementation is notoriously tricky
**Mitigation**:
- Study reference implementations (FCEUX, Nestopia)
- Use detailed NESDev wiki timing diagrams
- Test with games that stress IRQ (SMB3, Mega Man 3)

### Risk 2: Performance
**Risk**: May not achieve 60 FPS on all games
**Mitigation**:
- Profile early and often
- Use ARM-specific optimizations (NEON)
- Consider frame skip as last resort
- Optimize hot paths first (80/20 rule)

### Risk 3: Memory Constraints
**Risk**: Large ROMs + menu system may exceed available RAM
**Mitigation**:
- Free previous ROM before loading new one
- Use memory pools
- Monitor allocations carefully
- Consider streaming large ROMs (if necessary)

### Risk 4: File System Complexity
**Risk**: Circle FS integration may be more complex than expected
**Mitigation**:
- Research Circle samples thoroughly
- Consider integrating FatFs library directly
- Simplify to read-only initially
- Test with small file operations first

### Risk 5: Testing Coverage
**Risk**: Can't test every ROM - may have hidden bugs
**Mitigation**:
- Prioritize popular games
- Use automated test ROMs
- Create compatibility database
- Encourage community testing (if open source)

---

## Success Criteria

### Minimum Viable Product (MVP)
- [ ] Mappers 0, 1, 2, 3, 4, 7 fully functional
- [ ] SD card ROM loading works
- [ ] Menu system allows ROM selection
- [ ] At least 20 popular games work flawlessly
- [ ] Maintains 60 FPS on majority of games

### Full Release
- [ ] 10+ mappers implemented (90%+ game coverage)
- [ ] ROM browser with metadata
- [ ] Save state support
- [ ] Configuration system
- [ ] 50+ games tested and verified
- [ ] Performance optimized (consistent 60 FPS)
- [ ] Documentation complete
- [ ] CLAUDE.md updated with new architecture

### Stretch Goals
- [ ] Audio (APU) implementation
- [ ] Game Genie cheat support
- [ ] Screenshot capture
- [ ] Online leaderboards (via WiFi)
- [ ] ROM favorites/history
- [ ] UGUI-based graphical menu

---

## Resource Requirements

### Development Tools
- Raspberry Pi 3 Model B (hardware testing)
- SD card (FAT32 formatted)
- USB gamepad
- HDMI display
- Cross-compilation toolchain
- Logic analyzer (optional, for debugging)

### Reference Materials
- NESDev Wiki (https://wiki.nesdev.com/)
- No$ NES debugger documentation
- Mesen emulator source (reference implementation)
- FCEUX source code
- NES test ROM collection

### Team Structure
- 1 developer (can be solo project)
- Testing volunteers (optional)
- Code reviews (optional but recommended)

---

## Appendix A: Mapper Implementation Priority

### High Priority (Week 1-6)
1. Mapper 0 (NROM) - **FIX CRITICAL BUG**
2. Mapper 3 (CNROM) - Simple, many games
3. Mapper 4 (MMC3) - Complex but most important
4. Mapper 7 (AxROM) - Medium complexity

### Medium Priority (Week 7-12)
5. Mapper 9 (MMC2) - Punch-Out!!
6. Mapper 10 (MMC4) - Fire Emblem
7. Mapper 11 (Color Dreams)
8. Mapper 66 (GxROM)

### Low Priority (Week 13+)
9. Mapper 5 (MMC5) - Very complex, few games
10. Mapper 71 (Camerica)
11. Mapper 87
12. Others as needed

---

## Appendix B: NES Mapper Statistics

Based on No-Intro ROM set analysis:

| Mapper | Games | % Coverage | Complexity |
|--------|-------|------------|------------|
| 0 | 247 | 11.2% | Very Low |
| 1 | 680 | 30.8% | Medium |
| 2 | 270 | 12.2% | Low |
| 3 | 155 | 7.0% | Low |
| 4 | 599 | 27.1% | High |
| 7 | 61 | 2.8% | Low |
| 9 | 19 | 0.9% | Medium |
| 10 | 4 | 0.2% | Medium |
| 11 | 36 | 1.6% | Low |
| **Total** | **2071** | **93.8%** | - |

Implementing mappers 0-4, 7 covers **91.1%** of all NES games.

---

## Appendix C: Performance Targets

### Frame Timing Budget (16.67ms @ 60 FPS)

| Component | Target Time | % of Frame |
|-----------|-------------|------------|
| CPU Emulation | 8.0ms | 48% |
| PPU Rendering | 6.0ms | 36% |
| Input Processing | 0.5ms | 3% |
| Mapper Operations | 1.0ms | 6% |
| Menu/UI (when active) | 1.0ms | 6% |
| **Buffer** | 0.17ms | 1% |
| **Total** | 16.67ms | 100% |

### Optimization Priorities

1. **PPU pixel rendering** - Largest single bottleneck
2. **CPU instruction dispatch** - Called ~30K times/frame
3. **Memory access** - Every instruction
4. **Mapper bank switching** - Variable frequency
5. **Input polling** - Once per frame

---

## Document Maintenance

**Created**: 2025-11-15
**Last Updated**: 2025-11-15
**Author**: AI Assistant (Claude)
**Status**: Draft - Pending Review

This plan should be reviewed and updated:
- After completing each phase
- When encountering technical blockers
- When priorities change
- Monthly during active development

---

*This implementation plan provides a comprehensive roadmap for transforming the Frankenstein NES emulator into a production-ready multi-game system. Adjust timelines and priorities based on project constraints and goals.*
