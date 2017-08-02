#pragma once

#include "rom.h"

namespace Frankenstein {

class Nes;

class Ppu {
public:
    struct BytePair {
        u8 first;
        u8 second;
    };

    enum ControlFlags {
        LowerNameTable,         //Name table address, changes between the four name tables at 0x2000 (0), 0x2400 (1), 0x2800 (2) and 0x2C00 (3).
        UpperNameTable,
        Increment,              //Specifies amount to increment address by, either 1 if this is 0 or 32 if this is 1.
        SpriteTable,            //Identifies which pattern table sprites are stored in, either 0x0000 (0) or 0x1000 (1). 
        BackgroundTable,        //Identifies which pattern table the background is stored in, either 0x0000 (0) or 0x1000 (1). 
        SpriteSize,             //Specifies the size of sprites in pixels, 8x8 if this is 0, otherwise 8x16.
        SlaveMode,              //Changes PPU between master and slave modes.  This is not used by the NES.
        NMI                     //Indicates whether a NMI should occur upon V-Blank. 
    };
    
    enum MaskFlags {
        Monochrome,             //Indicates whether the system is in colour (0) or monochrome mode (1)
        ClipBackground,         //Specifies whether to clip the background, that is whether to hide the background in the left 8 pixels on screen (0) or to show them (1).
        ClipSprites,            //Specifies whether to clip the sprites, that is whether to hide sprites in the left 8 pixels on screen (0) or to show them (1).
        ShowBackground,         //If this is 0, the background should not be displayed.
        ShowSprites,            //If this is 0, sprites should not be displayed. 
        RedTint,                //Indicates background colour in monochrome mode or colour intensity in colour mode. 
        GreenTint,
        BlueTint
    };
    
    enum StatusFlags {
        IgnoreWrite = 4,            //If set, indicates that writes to VRAM should be ignored. 
        ScanlineSpriteCount = 5,    //Scanline sprite count, if set, indicates more than 8 sprites on the current scanline.
        SpriteZeroHit = 6,          //Sprite 0 hit flag, set when a non-transparent pixel of sprite 0 overlaps a non-transparent background pixel.
        VBlank = 7                  //Indicates whether V-Blank is occurring.
    };

    enum SpriteFlags {
        LowerColor,             //Most significant two bits of the color.
        UpperColor,
        Priority = 5,           //Indicates whether this sprite has priority over the background. 
        FlipHorizontal = 6,     //Indicates whether to flip the sprite horizontally. 
        FlipVertical = 7        //Indicates whether to flip the sprite vertically.
    };

    const u16 MirrorLookup[5][4]{
        { 0, 0, 1, 1 },
        { 0, 1, 0, 1 },
        { 0, 0, 0, 0 },
        { 1, 1, 1, 1 },
        { 0, 1, 2, 3 },
    };

#ifndef NotNative
    struct RGBColor {
        u32 red : 8, 
            green : 8, 
            blue : 8, 
            alpha : 8;
        
        RGBColor(): red(0), green(0), blue(0), alpha(0xFF) {
        }
        
        RGBColor(u8 red, u8 green, u8 blue) : red(red), green(green), blue(blue), alpha(0xFF) {
        }
    };
#else
    struct RGBColor {
        u32 blue : 8,
            green : 8,
            red : 8, 
            alpha : 8;
            
        
        RGBColor(): blue(0), green(0), red(0), alpha(0) {
        }
        
        RGBColor(u8 red, u8 green, u8 blue) : blue(blue), green(green), red(red),  alpha(0) {
        }
    };
#endif

    const RGBColor systemPalette[0x40] = {
 { 0x6a, 0x6d, 0x6a },
 { 0x00, 0x13, 0x80 },
 { 0x1e, 0x00, 0x8a },
 { 0x39, 0x00, 0x7a },
 { 0x55, 0x00, 0x56 },
 { 0x5a, 0x00, 0x18 },
 { 0x4f, 0x10, 0x00 },
 { 0x38, 0x21, 0x00 },
 { 0x21, 0x33, 0x00 },
 { 0x00, 0x3d, 0x00 },
 { 0x00, 0x40, 0x00 },
 { 0x00, 0x39, 0x24 },
 { 0x00, 0x2e, 0x55 },
 { 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00 },
 { 0xb9, 0xbc, 0xb9 },
 { 0x18, 0x50, 0xc7 },
 { 0x4b, 0x30, 0xe3 },
 { 0x73, 0x22, 0xd6 },
 { 0x95, 0x1f, 0xa9 },
 { 0x9d, 0x28, 0x5c },
 { 0x96, 0x3c, 0x00 },
 { 0x7a, 0x51, 0x00 },
 { 0x5b, 0x67, 0x00 },
 { 0x22, 0x77, 0x00 },
 { 0x02, 0x7e, 0x02 },
 { 0x00, 0x76, 0x45 },
 { 0x00, 0x6e, 0x8a },
 { 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00 },
 { 0xff, 0xff, 0xff },
 { 0x68, 0xa6, 0xff },
 { 0x92, 0x99, 0xff },
 { 0xb0, 0x85, 0xff },
 { 0xd9, 0x75, 0xfd },
 { 0xe3, 0x77, 0xb9 },
 { 0xe5, 0x8d, 0x68 },
 { 0xcf, 0xa2, 0x2c },
 { 0xb3, 0xaf, 0x0c },
 { 0x7b, 0xc2, 0x11 },
 { 0x55, 0xca, 0x47 },
 { 0x46, 0xcb, 0x81 },
 { 0x47, 0xc1, 0xc5 },
 { 0x4a, 0x4d, 0x4a },
 { 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00 },
 { 0xff, 0xff, 0xff },
 { 0xcc, 0xea, 0xff },
 { 0xdd, 0xde, 0xff },
 { 0xec, 0xda, 0xff },
 { 0xf8, 0xd7, 0xfe },
 { 0xfc, 0xd6, 0xf5 },
 { 0xfd, 0xdb, 0xcf },
 { 0xf9, 0xe7, 0xb5 },
 { 0xf1, 0xf0, 0xaa },
 { 0xda, 0xfa, 0xa9 },
 { 0xc9, 0xff, 0xbc },
 { 0xc3, 0xfb, 0xd7 },
 { 0xc4, 0xf6, 0xf6 },
 { 0xbe, 0xc1, 0xbe },
 { 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00 }
    };
    Nes& nes;

    RGBColor* front;
    RGBColor* back;
    
    u32 Cycle;      // 0-340
    u32 ScanLine;   // 0-261, 0-239=visible, 240=post, 241-260=vblank, 261=pre
    u64 Frame;      // frame counter

    // storage variables
    u8 paletteData[32];
    u8 nameTableData[2048];
    u8 oamData[256];
    u8 chrData[0x2000];

    // PPU registers
    u16 v;      // current vram address (15 bit)
    u16 t;      // temporary vram address (15 bit)
    u8 x;       // fine x scroll (3 bit)
    u8 w;       // write toggle (1 bit)
    u8 f;       // even/odd frame flag (1 bit)

    u8 reg;

    // NMI flags
    bool nmiOccurred;
    bool nmiOutput;
    bool nmiPrevious;
    bool vblankOccured;
    u8 nmiDelay;

    // background temporary variables
    u8 nameTableByte;
    u8 attributeTableByte;
    u8 lowTileByte;
    u8 highTileByte;
    u64 tileData;

    // sprite temporary variables
    u32 spriteCount;
    u32 spritePatterns[8];
    u8 spritePositions[8];
    u8 spritePriorities[8];
    u8 spriteIndexes[8];

    // $2000 PPUCTRL
    u8 flagNameTable;        // 0: $2000; 1: $2400; 2: $2800; 3: $2C00
    u8 flagIncrement;        // 0: add 1; 1: add 32
    u8 flagSpriteTable;      // 0: $0000; 1: $1000; ignored in 8x16 mode
    u8 flagBackgroundTable;  // 0: $0000; 1: $1000
    u8 flagSpriteSize;       // 0: 8x8; 1: 8x16
    u8 flagMasterSlave;      // 0: read EXT; 1: write EXT

    // $2001 PPUMASK
    u8 flagGrayscale;           // 0: color; 1: grayscale
    u8 flagShowLeftBackground;  // 0: hide; 1: show
    u8 flagShowLeftSprites;     // 0: hide; 1: show
    u8 flagShowBackground;      // 0: hide; 1: show
    u8 flagShowSprites;         // 0: hide; 1: show
    u8 flagRedTint;             // 0: normal; 1: emphasized
    u8 flagGreenTint;           // 0: normal; 1: emphasized
    u8 flagBlueTint;            // 0: normal; 1: emphasized

    // $2002 PPUSTATUS
    u8 flagSpriteZeroHit;
    u8 flagSpriteOverflow;

    // $2003 OAMADDR
    u8 oamAddress;

    // $2007 PPUDATA
    u8 bufferedData;  // for buffered reads

    explicit Ppu(Nes& pNes);

    void Reset();
    u8 Read(u16 address);
    void Write(u16 address, u8 value);
    u16 MirrorAddress(u8 mode, u16 address);
    u8 readPalette(u16 address);
    void writePalette(u16 address, u8 value);
    u8 readRegister(u16 address);
    void writeRegister(u16 address, u8 value);
    void writeControl(u8 value);
    void writeMask(u8 value);
    u8 readStatus();
    void writeOAMAddress(u8 value);
    u8 readOAMData();
    void writeOAMData(u8 value);
    void writeScroll(u8 value);
    void writeAddress(u8 value);
    u8 readData();
    void writeData(u8 value);
    void writeDMA(u8 value);
    void incrementX();
    void incrementY();
    void copyX();
    void copyY();
    void nmiChange();
    void setVerticalBlank();
    void clearVerticalBlank();
    void fetchNameTableByte();
    void fetchAttributeTableByte();
    void fetchLowTileByte();
    void fetchHighTileByte();
    void storeTileData();
    u32 fetchTileData();
    u8 backgroundPixel();
    BytePair spritePixel();
    void renderPixel();
    u32 fetchSpritePattern(u8 i, u32 row);
    void evaluateSprites();
    void tick();
    void Step();
};
}

