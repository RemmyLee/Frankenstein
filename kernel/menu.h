//
// menu.h
//
// NES ROM Selection Menu for Frankenstein Emulator
//
#ifndef _menu_h
#define _menu_h

#include <circle/screen.h>
#include <circle/types.h>
#include "../emulator/include/gamepad.h"
#include "../emulator/include/rom.h"

// ROM entry structure
struct RomEntry {
    const char* name;         // Display name
    const u8* data;          // Pointer to ROM data (embedded or loaded)
    u32 size;                // ROM size in bytes
    u8 mapper;               // Mapper number (for display)
    bool isEmbedded;         // True if compiled-in, false if from SD card
};

// Menu state
enum MenuState {
    MENU_STATE_ROM_LIST,     // Browsing ROM list
    MENU_STATE_LOADING,      // Loading selected ROM
    MENU_STATE_IN_GAME,      // Playing game
    MENU_STATE_ERROR         // Error state
};

class Menu {
public:
    Menu(CScreenDevice* pScreen);
    ~Menu();

    // Menu management
    void Show();                           // Display menu
    void Hide();                           // Hide menu
    void Update(Frankenstein::Gamepad& pad1);  // Handle input
    void Render();                         // Render current state

    // ROM management
    void AddRom(const char* name, const u8* data, u32 size, u8 mapper, bool embedded = true);
    RomEntry* GetSelectedRom();            // Get currently selected ROM
    int GetRomCount() const { return romCount; }

    // State
    MenuState GetState() const { return state; }
    void SetState(MenuState newState) { state = newState; }
    bool IsRomSelected() const { return romSelected; }
    void ResetSelection() { romSelected = false; }

private:
    CScreenDevice* screen;

    // ROM list
    static const int MAX_ROMS = 32;
    RomEntry roms[MAX_ROMS];
    int romCount;

    // Selection state
    int selectedIndex;
    int scrollOffset;
    bool romSelected;
    MenuState state;

    // Input debouncing
    bool prevUpPressed;
    bool prevDownPressed;
    bool prevAPressed;
    bool prevBPressed;
    bool prevStartPressed;

    // Rendering helpers
    void DrawBox(int x, int y, int width, int height, u32 color);
    void DrawText(int x, int y, const char* text, u32 color);
    void DrawRomList();
    void DrawStatusBar();
    void DrawLoadingScreen();
    void DrawErrorScreen(const char* message);

    // Colors (RGB)
    static const u32 COLOR_BG = 0x000020;          // Dark blue
    static const u32 COLOR_TEXT = 0xFFFFFF;        // White
    static const u32 COLOR_SELECTED = 0x00FF00;    // Green
    static const u32 COLOR_BORDER = 0x4040FF;      // Light blue
    static const u32 COLOR_HEADER = 0xFFFF00;      // Yellow
    static const u32 COLOR_ERROR = 0xFF0000;       // Red
};

#endif
