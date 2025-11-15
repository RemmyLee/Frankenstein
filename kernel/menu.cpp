//
// menu.cpp
//
// NES ROM Selection Menu Implementation
//
#include "menu.h"
#include <circle/string.h>

Menu::Menu(CScreenDevice* pScreen)
    : screen(pScreen)
    , romCount(0)
    , selectedIndex(0)
    , scrollOffset(0)
    , romSelected(false)
    , state(MENU_STATE_ROM_LIST)
    , prevUpPressed(false)
    , prevDownPressed(false)
    , prevAPressed(false)
    , prevBPressed(false)
    , prevStartPressed(false)
{
    // Initialize ROM list
    for (int i = 0; i < MAX_ROMS; i++) {
        roms[i].name = nullptr;
        roms[i].data = nullptr;
        roms[i].size = 0;
        roms[i].mapper = 0;
        roms[i].isEmbedded = false;
    }
}

Menu::~Menu()
{
}

void Menu::AddRom(const char* name, const u8* data, u32 size, u8 mapper, bool embedded)
{
    if (romCount >= MAX_ROMS) {
        return;  // List full
    }

    roms[romCount].name = name;
    roms[romCount].data = data;
    roms[romCount].size = size;
    roms[romCount].mapper = mapper;
    roms[romCount].isEmbedded = embedded;
    romCount++;
}

RomEntry* Menu::GetSelectedRom()
{
    if (selectedIndex >= 0 && selectedIndex < romCount) {
        return &roms[selectedIndex];
    }
    return nullptr;
}

void Menu::Show()
{
    state = MENU_STATE_ROM_LIST;
    romSelected = false;
}

void Menu::Hide()
{
    state = MENU_STATE_IN_GAME;
}

void Menu::Update(Frankenstein::Gamepad& pad1)
{
    if (state != MENU_STATE_ROM_LIST) {
        return;  // Only handle input in menu state
    }

    // Read current button states
    bool upPressed = pad1.buttons[Frankenstein::Gamepad::ButtonIndex::Up];
    bool downPressed = pad1.buttons[Frankenstein::Gamepad::ButtonIndex::Down];
    bool aPressed = pad1.buttons[Frankenstein::Gamepad::ButtonIndex::A];
    bool startPressed = pad1.buttons[Frankenstein::Gamepad::ButtonIndex::Start];

    // Up: Navigate up (with debounce)
    if (upPressed && !prevUpPressed) {
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
        }
    }

    // Down: Navigate down (with debounce)
    if (downPressed && !prevDownPressed) {
        if (selectedIndex < romCount - 1) {
            selectedIndex++;
            // Scroll if needed (show 10 items at a time)
            if (selectedIndex >= scrollOffset + 10) {
                scrollOffset = selectedIndex - 9;
            }
        }
    }

    // A or Start: Select ROM
    if ((aPressed && !prevAPressed) || (startPressed && !prevStartPressed)) {
        if (romCount > 0) {
            romSelected = true;
            state = MENU_STATE_LOADING;
        }
    }

    // Update previous button states
    prevUpPressed = upPressed;
    prevDownPressed = downPressed;
    prevAPressed = aPressed;
    prevStartPressed = startPressed;
}

void Menu::Render()
{
    switch (state) {
        case MENU_STATE_ROM_LIST:
            DrawRomList();
            DrawStatusBar();
            break;

        case MENU_STATE_LOADING:
            DrawLoadingScreen();
            break;

        case MENU_STATE_ERROR:
            DrawErrorScreen("Failed to load ROM");
            break;

        case MENU_STATE_IN_GAME:
            // Don't render menu while in game
            break;
    }
}

void Menu::DrawRomList()
{
    // Clear screen with background color
    // Note: CScreenDevice doesn't have direct color fill in all versions
    // We'll use Write with spaces to create colored areas

    // Draw title
    CString title("=== FRANKENSTEIN NES EMULATOR ===");
    screen->Write((const char*)title, title.GetLength());
    screen->Write("\n\n", 2);

    // Show ROM count
    CString info;
    info.Format("ROMs: %d  |  Coverage: 91.1%% (Mappers 0,1,2,3,4,7)\n\n", romCount);
    screen->Write((const char*)info, info.GetLength());

    // Draw ROM list (show 10 items at a time)
    int maxVisible = 10;
    int startIndex = scrollOffset;
    int endIndex = scrollOffset + maxVisible;
    if (endIndex > romCount) {
        endIndex = romCount;
    }

    for (int i = startIndex; i < endIndex; i++) {
        CString line;
        const char* indicator = (i == selectedIndex) ? ">" : " ";

        // Format: "> ROM Name                    [Mapper X]  XKB"
        u32 sizeKB = roms[i].size / 1024;
        line.Format("%s %-30s [Mapper %d]  %uKB\n",
                     indicator,
                     roms[i].name,
                     roms[i].mapper,
                     sizeKB);

        screen->Write((const char*)line, line.GetLength());
    }

    // Scroll indicator
    if (romCount > maxVisible) {
        screen->Write("\n", 1);
        CString scroll;
        scroll.Format("                    [Showing %d-%d of %d]\n",
                      startIndex + 1,
                      endIndex,
                      romCount);
        screen->Write((const char*)scroll, scroll.GetLength());
    }
}

void Menu::DrawStatusBar()
{
    screen->Write("\n", 1);
    screen->Write("----------------------------------------------------------------\n", 65);
    screen->Write("Controls: UP/DOWN=Navigate  A/START=Select\n", 44);
    screen->Write("In-Game:  SELECT+START=Return to Menu\n", 39);
}

void Menu::DrawLoadingScreen()
{
    screen->Write("\n\n\n\n", 4);
    screen->Write("                      Loading ROM...\n", 37);
    screen->Write("                      Please wait...\n", 37);
}

void Menu::DrawErrorScreen(const char* message)
{
    screen->Write("\n\n\n\n", 4);
    screen->Write("                        ERROR!\n", 31);
    CString msg;
    msg.Format("                    %s\n", message);
    screen->Write((const char*)msg, msg.GetLength());
    screen->Write("\n                  Press A to return\n", 37);
}

void Menu::DrawBox(int x, int y, int width, int height, u32 color)
{
    // Simple box drawing using characters
    // (Advanced version would use direct pixel access)
    // For now, we'll use the screen's built-in text rendering
}

void Menu::DrawText(int x, int y, const char* text, u32 color)
{
    // Simple text rendering
    // (Advanced version would handle positioning)
    screen->Write(text, strlen(text));
}
