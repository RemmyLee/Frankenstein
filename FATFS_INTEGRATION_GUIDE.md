# FatFs SD Card Integration Guide

## Overview

This guide provides step-by-step instructions for integrating the FatFs filesystem library to enable dynamic ROM loading from SD card on the Raspberry Pi 3.

**Current Status**: Menu system implemented with hardcoded ROMs (Phase 1 ✅)
**Next Step**: Add FatFs for SD card access (Phase 2)

---

## Prerequisites

### Required Tools
- FatFs library (download from http://elm-chan.org/fsw/ff/00index_e.html)
- ARM cross-compiler (arm-none-eabi-gcc)
- SD card (FAT32 formatted)
- Raspberry Pi 3 Model B

### Circle Library Components Needed
- `CDevice` - Device abstraction
- `CEMMCDevice` - SD card interface (in circle/addon/SDCard/)

---

## Step 1: Download and Extract FatFs

### Download FatFs R0.15
```bash
cd /path/to/Frankenstein/subprojects
mkdir fatfs
cd fatfs
wget http://elm-chan.org/fsw/ff/arc/ff15.zip
unzip ff15.zip
```

### Expected Directory Structure
```
subprojects/fatfs/
├── source/
│   ├── ff.c          # FatFs core
│   ├── ff.h          # FatFs header
│   ├── ffconf.h      # Configuration
│   ├── diskio.h      # Disk I/O interface
│   └── diskio.c      # Disk I/O template (we'll replace this)
└── documents/        # FatFs documentation
```

---

## Step 2: Configure FatFs (ffconf.h)

FatFs requires configuration for bare metal embedded systems. Create or modify `ffconf.h`:

```c
// ffconf.h - FatFs Configuration for Frankenstein Emulator

#ifndef FFCONF_DEF
#define FFCONF_DEF 80286

// Function Configurations
#define FF_FS_READONLY  1      // Read-only (we don't need writes for ROMs)
#define FF_FS_MINIMIZE  0      // Full function support
#define FF_USE_STRFUNC  1      // String functions
#define FF_USE_FIND     1      // Directory search functions
#define FF_USE_MKFS     0      // Don't need format
#define FF_USE_FASTSEEK 0      // Fast seek not needed
#define FF_USE_EXPAND   0      // Expand not needed
#define FF_USE_CHMOD    0      // Don't need chmod
#define FF_USE_LABEL    0      // Volume label not needed
#define FF_USE_FORWARD  0      // Stream forward not needed

// Locale and Namespace
#define FF_CODE_PAGE    437    // ASCII
#define FF_USE_LFN      1      // Long filename support (1 = static buffer)
#define FF_MAX_LFN      255    // Max long filename length
#define FF_LFN_UNICODE  0      // ASCII only
#define FF_LFN_BUF      255    // LFN buffer size
#define FF_SFN_BUF      12     // Short filename buffer
#define FF_STRF_ENCODE  0      // ASCII
#define FF_FS_RPATH     0      // Relative path not needed
#define FF_FS_NORTC     1      // No real-time clock
#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2025

// Volume Management
#define FF_VOLUMES      1      // 1 SD card
#define FF_STR_VOLUME_ID 0
#define FF_MULTI_PARTITION 0   // Single partition
#define FF_MIN_SS       512    // Sector size
#define FF_MAX_SS       512
#define FF_USE_TRIM     0      // No TRIM
#define FF_FS_NOFSINFO  0      // Use FSInfo

// System Configurations
#define FF_FS_TINY      0      // Full buffer
#define FF_FS_EXFAT     0      // FAT32 only
#define FF_FS_LOCK      0      // No file lock
#define FF_FS_REENTRANT 0      // Single threaded
#define FF_FS_TIMEOUT   1000
#define FF_SYNC_t       void*

#include <stdint.h>

#endif
```

**Key Settings Explained**:
- `FF_FS_READONLY=1` - We only read ROMs, no writes needed
- `FF_USE_LFN=1` - Support long filenames (e.g., "Super Mario Bros.nes")
- `FF_FS_NORTC=1` - No real-time clock (bare metal)
- `FF_VOLUMES=1` - Single SD card

---

## Step 3: Implement Disk I/O Layer (diskio.c)

FatFs needs a platform-specific disk I/O layer. Create `diskio.c` that interfaces with Circle's EMMC device:

```c
// diskio.c - Disk I/O layer for Circle EMMC

#include "ff.h"
#include "diskio.h"

// External Circle EMMC device (defined in kernel)
extern "C" {
    void* g_emmc_device;  // Pointer to CEMMCDevice
}

// Disk Status
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    // TODO: Check Circle EMMC device status
    if (g_emmc_device == nullptr) {
        return STA_NOINIT;
    }

    return 0;  // OK
}

// Initialize Disk
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;

    // Circle EMMC is initialized in kernel
    // Just verify it's ready
    return disk_status(pdrv);
}

// Read Sectors
DRESULT disk_read(
    BYTE pdrv,      // Physical drive
    BYTE *buff,     // Data buffer
    LBA_t sector,   // Sector address
    UINT count      // Sector count
)
{
    if (pdrv != 0) return RES_PARERR;
    if (g_emmc_device == nullptr) return RES_NOTRDY;

    // TODO: Call Circle EMMC read function
    // CEMMCDevice::read(sector, count, buff)
    // For now, return error
    return RES_ERROR;
}

// Write Sectors (not needed for read-only)
DRESULT disk_write(
    BYTE pdrv,
    const BYTE *buff,
    LBA_t sector,
    UINT count
)
{
    return RES_WRPRT;  // Write protected (read-only mode)
}

// Disk I/O Control
DRESULT disk_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff
)
{
    if (pdrv != 0) return RES_PARERR;

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            // TODO: Get SD card size from Circle EMMC
            *(LBA_t*)buff = 0;  // Placeholder
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;  // 1 sector
            return RES_OK;
    }

    return RES_PARERR;
}
```

**Important**: This needs to be connected to Circle's `CEMMCDevice` class. See Step 5 for kernel integration.

---

## Step 4: Add FatFs to Build System

Create `subprojects/fatfs/meson.build`:

```python
# FatFs Library Build Configuration

fatfs_sources = files(
    'source/ff.c',
    'diskio.c'  # Our custom disk I/O layer
)

fatfs_include = include_directories('source')

fatfs_lib = static_library('fatfs',
    fatfs_sources,
    include_directories: fatfs_include,
    c_args: ['-DFF_DEFINED']
)

fatfs_dep = declare_dependency(
    link_with: fatfs_lib,
    include_directories: fatfs_include
)
```

Update root `meson.build` to include FatFs:

```python
# In root meson.build, add after Circle subproject:

if meson.is_cross_build() and host_machine.system() == 'raspberry-pi'
    fatfs = subproject('fatfs')
    fatfs_dep = fatfs.get_variable('fatfs_dep')
endif
```

Update `kernel/meson.build` to link FatFs:

```python
elf = executable('kernel.elf',
    'startup.S', 'main.cpp', 'kernel.cpp', 'menu.cpp', 'sdcard.cpp',  # Add sdcard.cpp
    dependencies: [libcircle_dep, emulator_dep, fatfs_dep],  # Add fatfs_dep
    cpp_args: cpp_cross_args,
    link_args: cpp_cross_args
)
```

---

## Step 5: Kernel Integration

### Add SD Card Manager (kernel/sdcard.h)

```cpp
// sdcard.h - SD Card and Filesystem Manager

#ifndef _sdcard_h
#define _sdcard_h

#include <circle/types.h>
#include "menu.h"

// Forward declarations
class CEMMCDevice;
extern "C" {
    #include "../subprojects/fatfs/source/ff.h"
}

class SDCardManager {
public:
    SDCardManager();
    ~SDCardManager();

    bool Initialize(CEMMCDevice* emmc);
    bool ScanROMs(Menu* menu);

    u8* LoadROM(const char* filename, u32* size);
    void FreeROM(u8* data);

private:
    CEMMCDevice* emmc_device;
    FATFS filesystem;
    bool mounted;

    bool mountFilesystem();
    bool isNESFile(const char* filename);
    u8 getROMMapper(const u8* header);
};

#endif
```

### Implement SD Card Manager (kernel/sdcard.cpp)

```cpp
// sdcard.cpp - SD Card and Filesystem Manager Implementation

#include "sdcard.h"
#include <circle/logger.h>
#include <circle/util.h>
#include <cstring>

static const char FromSDCard[] = "sdcard";

SDCardManager::SDCardManager()
    : emmc_device(nullptr)
    , mounted(false)
{
}

SDCardManager::~SDCardManager()
{
    if (mounted) {
        f_unmount("0:");
    }
}

bool SDCardManager::Initialize(CEMMCDevice* emmc)
{
    emmc_device = emmc;

    if (emmc_device == nullptr) {
        CLogger::Get()->Write(FromSDCard, LogError, "EMMC device is null");
        return false;
    }

    return mountFilesystem();
}

bool SDCardManager::mountFilesystem()
{
    FRESULT result = f_mount(&filesystem, "0:", 1);

    if (result != FR_OK) {
        CLogger::Get()->Write(FromSDCard, LogError,
            "Failed to mount filesystem: %d", result);
        return false;
    }

    mounted = true;
    CLogger::Get()->Write(FromSDCard, LogNotice, "Filesystem mounted successfully");
    return true;
}

bool SDCardManager::ScanROMs(Menu* menu)
{
    if (!mounted) {
        CLogger::Get()->Write(FromSDCard, LogError, "Filesystem not mounted");
        return false;
    }

    DIR dir;
    FILINFO fileinfo;

    // Open /roms/ directory
    FRESULT result = f_opendir(&dir, "/roms");
    if (result != FR_OK) {
        // Try root directory if /roms doesn't exist
        result = f_opendir(&dir, "/");
        if (result != FR_OK) {
            CLogger::Get()->Write(FromSDCard, LogError,
                "Failed to open directory: %d", result);
            return false;
        }
    }

    CLogger::Get()->Write(FromSDCard, LogNotice, "Scanning for ROMs...");

    int romCount = 0;
    while (true) {
        result = f_readdir(&dir, &fileinfo);

        if (result != FR_OK || fileinfo.fname[0] == 0) {
            break;  // End of directory
        }

        // Check if it's a .nes file
        if (isNESFile(fileinfo.fname)) {
            // Load ROM to get mapper info
            u32 size;
            u8* data = LoadROM(fileinfo.fname, &size);

            if (data != nullptr && size >= 16) {
                // Parse iNES header (first 16 bytes)
                u8 mapper = getROMMapper(data);

                // Add to menu
                // Note: This stores the filename, not the data
                // Data will be loaded when ROM is selected
                menu->AddROM(fileinfo.fname, nullptr, size, mapper, false);

                FreeROM(data);
                romCount++;

                CLogger::Get()->Write(FromSDCard, LogNotice,
                    "Found ROM: %s (Mapper %d, %u KB)",
                    fileinfo.fname, mapper, size / 1024);
            }
        }
    }

    f_closedir(&dir);

    CLogger::Get()->Write(FromSDCard, LogNotice,
        "Scan complete: %d ROMs found", romCount);

    return romCount > 0;
}

bool SDCardManager::isNESFile(const char* filename)
{
    int len = strlen(filename);
    if (len < 5) return false;

    // Check for .nes extension (case insensitive)
    const char* ext = &filename[len - 4];
    return (strcasecmp(ext, ".nes") == 0);
}

u8 SDCardManager::getROMMapper(const u8* header)
{
    // iNES header format:
    // Byte 6: flags 6 (mapper low nibble)
    // Byte 7: flags 7 (mapper high nibble)
    u8 flags6 = header[6];
    u8 flags7 = header[7];

    u8 mapper = ((flags7 & 0xF0) | (flags6 >> 4));
    return mapper;
}

u8* SDCardManager::LoadROM(const char* filename, u32* size)
{
    FIL file;
    FRESULT result;

    // Open file
    result = f_open(&file, filename, FA_READ);
    if (result != FR_OK) {
        CLogger::Get()->Write(FromSDCard, LogError,
            "Failed to open ROM: %s (%d)", filename, result);
        return nullptr;
    }

    // Get file size
    *size = f_size(&file);

    // Allocate memory
    u8* data = new u8[*size];
    if (data == nullptr) {
        CLogger::Get()->Write(FromSDCard, LogError,
            "Failed to allocate memory for ROM: %u bytes", *size);
        f_close(&file);
        return nullptr;
    }

    // Read file
    UINT bytesRead;
    result = f_read(&file, data, *size, &bytesRead);
    if (result != FR_OK || bytesRead != *size) {
        CLogger::Get()->Write(FromSDCard, LogError,
            "Failed to read ROM: %s (%d)", filename, result);
        delete[] data;
        f_close(&file);
        return nullptr;
    }

    f_close(&file);

    CLogger::Get()->Write(FromSDCard, LogNotice,
        "Loaded ROM: %s (%u bytes)", filename, *size);

    return data;
}

void SDCardManager::FreeROM(u8* data)
{
    delete[] data;
}
```

### Update Kernel (kernel/kernel.h)

```cpp
// Add to kernel.h

#include "menu.h"
#include "sdcard.h"  // NEW
#include <circle/device.h>
#include <circle/emmc.h>  // NEW

class CKernel {
    // ... existing code ...

private:
    // ... existing Circle devices ...

    CEMMCDevice m_EMMC;       // NEW - SD card interface
    SDCardManager sdcard;     // NEW - Filesystem manager
    Menu menu;
    // ... rest of code ...
};
```

### Update Kernel Implementation (kernel/kernel.cpp)

```cpp
// Modify constructor to initialize EMMC
CKernel::CKernel(void)
    : m_Screen(m_Options.GetWidth(), m_Options.GetHeight())
    , m_Timer(&m_Interrupt)
    , m_Logger(m_Options.GetLogLevel(), &m_Timer)
    , m_DWHCI(&m_Interrupt, &m_Timer)
    , m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED)  // NEW
    , sdcard()                                     // NEW
    , menu(&m_Screen)
    // ... rest
{
    // ... existing initialization ...
}

// Modify Initialize() to initialize EMMC and filesystem
boolean CKernel::Initialize(void)
{
    // ... existing initialization ...

    // Initialize EMMC (SD card)
    if (bOK) {
        bOK = m_EMMC.Initialize();
        if (!bOK) {
            m_Logger.Write(FromKernel, LogError, "EMMC initialization failed");
        }
    }

    // Initialize filesystem
    if (bOK) {
        bOK = sdcard.Initialize(&m_EMMC);
        if (!bOK) {
            m_Logger.Write(FromKernel, LogWarning,
                "Filesystem init failed - using embedded ROMs only");
            bOK = TRUE;  // Continue anyway with embedded ROMs
        }
    }

    return bOK;
}

// Modify Run() to scan ROMs from SD card
TShutdownMode CKernel::Run(void)
{
    // ... USB gamepad initialization ...

    m_Logger.Write(FromKernel, LogNotice, "Scanning SD card for ROMs...");

    // Scan ROMs from SD card
    bool foundROMs = sdcard.ScanROMs(&menu);

    if (!foundROMs) {
        m_Logger.Write(FromKernel, LogWarning,
            "No ROMs found on SD card - using embedded ROM");

        // Add embedded ROM as fallback
        menu.AddRom("Embedded Test ROM",
                    Frankenstein::StaticRom::raw,
                    Frankenstein::StaticRom::length,
                    embedded_rom.GetMapper(), true);
    }

    // ... rest of main loop ...
}
```

---

## Step 6: Update Menu for Dynamic Loading

### Modify Menu to Store Filenames

Update `menu.h`:

```cpp
struct RomEntry {
    const char* name;         // Display name or filename
    const char* filename;     // Full path (for SD card ROMs)
    const u8* data;          // Pointer to ROM data (null if not loaded)
    u32 size;                // ROM size in bytes
    u8 mapper;               // Mapper number
    bool isEmbedded;         // True if compiled-in
    bool isLoaded;           // True if data is loaded in memory
};
```

Update `menu.cpp`:

```cpp
void Menu::AddRom(const char* name, const char* filename,
                  u32 size, u8 mapper, bool embedded)
{
    if (romCount >= MAX_ROMS) return;

    roms[romCount].name = name;
    roms[romCount].filename = filename;  // Store filename
    roms[romCount].data = nullptr;       // Will load on demand
    roms[romCount].size = size;
    roms[romCount].mapper = mapper;
    roms[romCount].isEmbedded = embedded;
    roms[romCount].isLoaded = false;
    romCount++;
}
```

---

## Step 7: SD Card Setup

### Format SD Card
1. Format SD card as FAT32 (not exFAT)
2. Create `/roms/` directory
3. Copy .nes ROM files to `/roms/`

### Example SD Card Structure
```
/
├── roms/
│   ├── Super Mario Bros.nes
│   ├── Donkey Kong.nes
│   ├── Zelda.nes
│   ├── Metroid.nes
│   └── Castlevania.nes
├── bootcode.bin
├── start.elf
├── fixup.dat
├── config.txt
└── kernel7.img
```

**Note**: Boot files and kernel7.img go in root, ROMs go in `/roms/`

---

## Step 8: Testing

### Build and Deploy
```bash
# Cross-compile
meson build-rpi --cross-file subprojects/circle/rpi3-cross.txt
ninja -C build-rpi

# Copy to SD card
cp build-rpi/kernel/kernel7.img /path/to/sdcard/
cp subprojects/circle/boot/* /path/to/sdcard/
```

### Expected Boot Sequence
1. Raspberry Pi boots
2. Kernel initializes EMMC
3. FatFs mounts filesystem
4. Scans `/roms/` directory
5. Displays menu with all found ROMs
6. User navigates and selects ROM
7. ROM loads from SD card
8. Emulation begins

### Debug Output (Serial Console)
```
[kernel] Compile time: Nov 15 2025 10:30:00
[kernel] Starting Frankenstein NES Emulator...
[sdcard] Filesystem mounted successfully
[sdcard] Scanning for ROMs...
[sdcard] Found ROM: Super Mario Bros.nes (Mapper 0, 40 KB)
[sdcard] Found ROM: Donkey Kong.nes (Mapper 0, 16 KB)
[sdcard] Found ROM: Zelda.nes (Mapper 1, 128 KB)
[sdcard] Scan complete: 3 ROMs found
[kernel] ROM loaded, starting emulation...
```

---

## Troubleshooting

### No ROMs Found
- Check SD card is formatted as FAT32
- Verify `/roms/` directory exists
- Check ROM files have `.nes` extension
- Check serial output for errors

### Mount Failed
- Verify EMMC device initialized
- Check SD card is properly inserted
- Try reformatting SD card
- Check diskio.c implementation

### ROMs Don't Load
- Verify iNES header is valid
- Check ROM file isn't corrupted
- Verify mapper is supported (0,1,2,3,4,7)
- Check memory allocation succeeds

### Performance Issues
- Ensure `-O3` optimization enabled
- Profile ROM loading time
- Consider caching loaded ROMs
- Check SD card speed (Class 10 recommended)

---

## Future Enhancements

### ROM Caching
- Keep last played ROM in memory
- Implement LRU cache for multiple ROMs
- Preload next/previous ROM in list

### Save States
- Store save states to SD card
- One save per ROM
- Quick save/load with controller combo

### ROM Metadata
- Display ROM screenshots
- Show game title from database
- Display play time statistics

### Folder Support
- Support subdirectories (by genre, etc.)
- Breadcrumb navigation
- Favorites system

---

## Summary

This integration adds full SD card support to the Frankenstein emulator:

**What's Added**:
- FatFs filesystem library
- SD card reading via Circle EMMC
- Dynamic ROM discovery and loading
- Memory management for multiple ROMs

**User Experience**:
1. Copy ROMs to SD card `/roms/` folder
2. Boot Raspberry Pi
3. Browse all ROMs in menu
4. Select and play
5. Switch games anytime

**Development Time**: ~6-8 hours for complete integration

**Next Steps**: After testing, add save states and configuration system.
