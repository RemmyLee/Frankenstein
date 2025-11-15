//
// kernel.cpp
//
#include "kernel.h"
static const char FromKernel[] = "kernel";

Nes* CKernel::s_nes = nullptr;
CLogger* CKernel::s_logger = nullptr;
CInterruptSystem* CKernel::s_interrupt = nullptr;
TGamePadState CKernel::s_input_player1;
TGamePadState CKernel::s_input_player2;

CKernel::CKernel(void)
    : m_Screen(m_Options.GetWidth(), m_Options.GetHeight())
    , m_Timer(&m_Interrupt)
    , m_Logger(m_Options.GetLogLevel(), &m_Timer)
    , m_DWHCI(&m_Interrupt, &m_Timer)
    , menu(&m_Screen)
    , current_rom(nullptr)
    , current_nes(nullptr)
    , in_menu(true)
    , prev_select_pressed(false)
    , prev_start_pressed(false)
    , embedded_rom(Frankenstein::StaticRom::raw, Frankenstein::StaticRom::length)
    , nes(embedded_rom, &m_Screen)
{
    CKernel::s_logger = &m_Logger;
    CKernel::s_interrupt = &m_Interrupt;

    CKernel::s_input_player1.axes[0].value = 0;
    CKernel::s_input_player1.axes[1].value = 0;
    CKernel::s_input_player1.buttons = 0;

    CKernel::s_input_player2.axes[0].value = 0;
    CKernel::s_input_player2.axes[1].value = 0;
    CKernel::s_input_player2.buttons = 0;

    // Add hardcoded test ROMs to menu
    // For now, we'll add the embedded ROM as a test entry
    // TODO: Add more test ROMs with different mappers when we have filesystem support
    menu.AddRom("Embedded Test ROM", Frankenstein::StaticRom::raw,
                Frankenstein::StaticRom::length, embedded_rom.GetMapper(), true);
}

CKernel::~CKernel(void)
{
}

boolean CKernel::Initialize(void)
{
    boolean bOK = TRUE;

    if (bOK) {
        bOK = m_Screen.Initialize();
    }

    if (bOK) {
        bOK = m_Serial.Initialize(115200);
    }

    if (bOK) {
        CDevice* pTarget = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (pTarget == 0) {
            pTarget = &m_Screen;
        }

        bOK = m_Logger.Initialize(pTarget);
        CKernel::s_logger = &m_Logger;
    }

    if (bOK) {
        bOK = m_Interrupt.Initialize();
    }

    if (bOK) {
        bOK = m_Timer.Initialize();
    }

    if (bOK) {
        bOK = m_DWHCI.Initialize();
    }

    // TODO: call Initialize () of added members here (if required)

    return bOK;
}

TShutdownMode CKernel::Run(void)
{
    m_Logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

    boolean bFound = FALSE;

    // Initialize USB gamepad
    for (unsigned nDevice = 1; 1; nDevice++) {
        CUSBGamePadDevice* pGamePad = nullptr;
        const TGamePadState* pState = nullptr;

        CString DeviceName;
        DeviceName.Format("upad%u", nDevice);

        pGamePad = (CUSBGamePadDevice*)m_DeviceNameService.GetDevice(DeviceName, FALSE);
        if (pGamePad == nullptr) {
            break;
        }

        pState = pGamePad->GetReport();
        if (pState == 0) {
            m_Logger.Write(FromKernel, LogError, "Cannot get report from %s",
                (const char*)DeviceName);

            continue;
        }

        m_Logger.Write(FromKernel, LogNotice, "Gamepad %u: %d Button(s) %d Hat(s)",
            nDevice, pState->nbuttons, pState->nhats);

        for (int i = 0; i < pState->naxes; i++) {
            m_Logger.Write(FromKernel, LogNotice, "Gamepad %u: Axis %d: Minimum %d Maximum %d",
                nDevice, i + 1, pState->axes[i].minimum, pState->axes[i].maximum);
        }

        pGamePad->RegisterStatusHandler(GamePadStatusHandler);
        bFound = TRUE;
    }

    if (!bFound) {
        m_Logger.Write(FromKernel, LogPanic, "Gamepad not found");
    }

    m_Logger.Write(FromKernel, LogNotice, "Starting Frankenstein NES Emulator...");
    m_Logger.Write(FromKernel, LogNotice, "Mapper Coverage: 91.1%% (0,1,2,3,4,7)");

    // Initialize with embedded ROM
    current_rom = &embedded_rom;
    current_nes = &nes;

    // Show menu on startup
    menu.Show();
    in_menu = true;

    // Update gamepad state for menu
    Gamepad menu_pad;

    // Main loop
    while (true) {
        if (in_menu) {
            // Menu mode - handle menu input and rendering

            // Update gamepad state
            menu_pad.buttons[Gamepad::ButtonIndex::A]      = s_input_player1.buttons & 0x80;
            menu_pad.buttons[Gamepad::ButtonIndex::B]      = s_input_player1.buttons & 0x40;
            menu_pad.buttons[Gamepad::ButtonIndex::Select] = s_input_player1.buttons & 0x10;
            menu_pad.buttons[Gamepad::ButtonIndex::Start]  = s_input_player1.buttons & 0x20;
            menu_pad.buttons[Gamepad::ButtonIndex::Up]     = !s_input_player1.axes[1].value;
            menu_pad.buttons[Gamepad::ButtonIndex::Down]   = s_input_player1.axes[1].value == 255;
            menu_pad.buttons[Gamepad::ButtonIndex::Left]   = !s_input_player1.axes[0].value;
            menu_pad.buttons[Gamepad::ButtonIndex::Right]  = s_input_player1.axes[0].value == 255;

            // Update menu with input
            menu.Update(menu_pad);

            // Render menu
            menu.Render();

            // Check if user selected a ROM
            if (menu.IsRomSelected()) {
                RomEntry* selected = menu.GetSelectedRom();
                if (selected != nullptr) {
                    m_Logger.Write(FromKernel, LogNotice, "Loading ROM: %s", selected->name);

                    // For now, we only have the embedded ROM, so we just hide the menu
                    menu.Hide();
                    in_menu = false;

                    m_Logger.Write(FromKernel, LogNotice, "ROM loaded, starting emulation...");
                }
            }

            // Small delay to prevent menu from being too fast
            m_Timer.MsDelay(16);  // ~60 FPS menu update
        }
        else {
            // Game mode - run emulator
            current_nes->Step();

            if (current_nes->cpu.nmiOccurred) {
                // Update gamepad state
                current_nes->pad1.buttons[Gamepad::ButtonIndex::A]      = s_input_player1.buttons & 0x80;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::B]      = s_input_player1.buttons & 0x40;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::Select] = s_input_player1.buttons & 0x10;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::Start]  = s_input_player1.buttons & 0x20;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::Up]     = !s_input_player1.axes[1].value;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::Down]   = s_input_player1.axes[1].value == 255;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::Left]   = !s_input_player1.axes[0].value;
                current_nes->pad1.buttons[Gamepad::ButtonIndex::Right]  = s_input_player1.axes[0].value == 255;

                current_nes->pad2.buttons[Gamepad::ButtonIndex::A]      = s_input_player2.buttons & 0x80;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::B]      = s_input_player2.buttons & 0x40;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::Select] = s_input_player2.buttons & 0x10;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::Start]  = s_input_player2.buttons & 0x20;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::Up]     = !s_input_player2.axes[1].value;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::Down]   = s_input_player2.axes[1].value == 255;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::Left]   = !s_input_player2.axes[0].value;
                current_nes->pad2.buttons[Gamepad::ButtonIndex::Right]  = s_input_player2.axes[0].value == 255;

                // Check for SELECT + START to return to menu
                bool select_pressed = s_input_player1.buttons & 0x10;
                bool start_pressed = s_input_player1.buttons & 0x20;

                if (select_pressed && start_pressed && !prev_select_pressed && !prev_start_pressed) {
                    m_Logger.Write(FromKernel, LogNotice, "Returning to menu...");
                    menu.Show();
                    in_menu = true;
                }

                prev_select_pressed = select_pressed;
                prev_start_pressed = start_pressed;

                m_Interrupt.EnableIRQ(ARM_IRQ_USB);
            }
        }
    }
    return ShutdownHalt;
}

void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState* pState)
{
    if(nDeviceIndex == 0) {
        s_input_player1 = *pState;
    }
    else if (nDeviceIndex == 1) {
        s_input_player2 = *pState;
    }
    s_interrupt->DisableIRQ(ARM_IRQ_USB);
}

