#include "gamepad.h"

namespace Frankenstein {

Gamepad::Gamepad()
    : index(0)
    , strobe(0)
    , buttons{false}
{
}

u8 Gamepad::Read()
{
    u8 value = 0;
    if (index > 8) {
        value = 0;
    }
    else if (buttons[index]) {
        value = 1;
    }
    index++;
    if ((strobe & 1) == 1) {
        index = 0;
    }
    return value;
}

void Gamepad::Write(u8 value)
{
    strobe = value;
    if ((strobe & 1) == 1) {
        index = 0;
    }
}
}
