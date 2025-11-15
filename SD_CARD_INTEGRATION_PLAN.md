# SD Card & File System Integration Plan

## Current Status - Research Findings

### What We Have ✅
- **Circle SD Card Support**: `addon/SDCard/emmc.h` provides low-level SD card access
- **Sample Code**: Examples show FAT filesystem usage in `addon/SDCard/sample/`
- **Reference Implementation**: Circle samples use `CFATFileSystem` class

### What's Missing ❌
- **FAT Filesystem Headers**: `<circle/fs/fat/fatfs.h>` referenced but NOT present
- **Filesystem Library**: Circle lib doesn't include filesystem implementation
- **Build Integration**: No FAT library in current Circle subproject

### Why This Matters
The samples reference a FAT filesystem that doesn't exist in our Circle version. This means:
1. Circle FAT support may be in a newer version
2. It's a separate library not included in the submodule
3. We need to add it ourselves

---

## Options for Moving Forward

### **Option A: Integrate FatFs Library** (Recommended)
**FatFs** by ChaN - Industry-standard embedded FAT library

**Pros:**
- Widely used, battle-tested
- Small footprint (~40KB code)
- No dynamic allocation (perfect for bare metal)
- Easy API
- BSD-style license

**Cons:**
- Requires integration work (2-4 hours)
- Need to write glue code for Circle's EMMC device

**Implementation Steps:**
1. Download FatFs from http://elm-chan.org/fsw/ff/00index_e.html
2. Add to `subprojects/fatfs/`
3. Create `diskio.c` glue layer for Circle EMMC
4. Update kernel to mount filesystem
5. Implement ROM scanner

**Estimated Time**: 4-6 hours

---

### **Option B: Raw Sector Reading** (Faster, Limited)
Read SD card sectors directly and parse FAT32 manually

**Pros:**
- No external dependencies
- Full control
- Faster to implement basic functionality

**Cons:**
- Only supports FAT32
- More fragile
- Limited features (no write support)
- More code to maintain

**Implementation Steps:**
1. Read Master Boot Record (sector 0)
2. Parse partition table
3. Read FAT32 boot sector
4. Parse directory entries
5. Read file data

**Estimated Time**: 6-8 hours (but simpler code)

---

### **Option C: Update Circle Subproject** (Clean, Time-Consuming)
Update to newer Circle version with built-in FAT support

**Pros:**
- Official support
- Well integrated
- Future updates easier

**Cons:**
- May break existing code
- Circle version compatibility unknown
- Could take significant time to test
- Risky at this stage

**Estimated Time**: 4-8 hours + testing

---

### **Option D: Simplified ROM Loader** (MVP Approach)
Create a minimal ROM loader without full filesystem

**Pros:**
- Fastest to implement
- Gets us to playable state quickly
- Can add filesystem later

**Cons:**
- Less user-friendly
- Limited to single ROM or predefined list
- Not the final solution

**Implementation Steps:**
1. Hardcode ROM filenames or use sector addresses
2. Read ROM data directly from known sectors
3. Load into memory
4. Works for testing but not production

**Estimated Time**: 2-3 hours

---

## Recommendation

**For Now (MVP): Option D - Simplified ROM Loader**
- Get emulator working with multiple ROMs quickly
- Hardcode 5-10 ROM sector addresses
- Test mapper implementations with real games
- Validate performance

**Next Phase: Option A - FatFs Integration**
- Proper filesystem support
- Dynamic ROM discovery
- Production-ready solution
- Menu-driven ROM selection

---

## Alternative: Focus on Menu System First

Since filesystem integration is blocked, we could:

### **Skip to Phase 3: Menu System Design**
Implement the menu/UI system NOW with:
- Hardcoded ROM list (for testing)
- Menu navigation
- ROM selection interface
- Input handling

**Benefits:**
- Gets UI working
- Tests rendering and input
- Can plug in filesystem later
- Makes progress on user experience

**Implementation:**
1. Design menu renderer (text-based)
2. Create ROM list structure (hardcoded initially)
3. Implement D-pad navigation
4. ROM selection and loading
5. Return-to-menu functionality

---

## Recommended Next Steps

### **Path Forward: Menu First, Filesystem Second**

**Phase 3A: Menu System (4-6 hours)**
1. Create menu renderer class
2. Hardcode 5-10 test ROMs (different mappers)
3. Implement navigation and selection
4. Test ROM loading/switching

**Phase 3B: Filesystem Integration (4-6 hours)**
1. Integrate FatFs library
2. Replace hardcoded list with dynamic scanning
3. Add subdirectory support
4. Implement ROM metadata extraction

**Phase 4: Performance & Polish**
1. Optimize rendering
2. Add save states
3. Configuration system

---

## Sample Code: Menu System (Hardcoded)

```cpp
// menu.h
struct RomEntry {
    const char* name;
    const u8* data;  // Pointer to embedded ROM
    u32 size;
    u8 mapper;
};

class MenuSystem {
public:
    void Render(CScreenDevice* screen);
    void HandleInput(Gamepad& pad);
    RomEntry* GetSelectedRom();

private:
    RomEntry romList[10];
    int selectedIndex;
    int scrollOffset;
};
```

---

## What to Implement Now?

I can proceed with any of these approaches. My recommendation:

**Start with Menu System (Phase 3)**
- Doesn't require filesystem
- Shows immediate progress
- Tests UI/rendering
- Can use embedded test ROMs
- Filesystem can be added later without rewriting menu code

**Then Add Filesystem (Phase 3B)**
- FatFs integration
- Dynamic ROM discovery
- Professional solution

---

## Questions for You

1. **Priority**: Menu system first, or filesystem integration first?
2. **Filesystem Approach**: Which option (A, B, C, or D)?
3. **Scope**: MVP with hardcoded ROMs, or full filesystem now?

**My Recommendation**: Build menu with hardcoded ROMs (2-3 hours), then add FatFs (4-6 hours). This gets you to a playable multi-ROM emulator fastest.

Let me know which path you'd like to take!
