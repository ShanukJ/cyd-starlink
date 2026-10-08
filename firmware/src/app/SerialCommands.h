#pragma once

#include "../hardware/Board.h"
#include "../ui/LvglPort.h"
#include "../ui/UiManager.h"

namespace app {

// Developer commands over the USB serial console (115200 baud), one per line:
//
//   screenshot   dump the displayed frame (see scripts/screenshot.py)
//   page <n>     show page n (0 dashboard, 1 history, 2 alignment, 3 diagnostics)
//   rotate <r>   set display rotation 0..3 (not saved)
//
// Polled from the UI task, so a command runs between LVGL frames.
class SerialCommands {
public:
    SerialCommands(hw::Board& board, ui::UiManager& ui, ui::LvglPort& port) : _board(board), _ui(ui), _port(port) {}
    void poll();

private:
    void screenshot();

    hw::Board& _board;
    ui::UiManager& _ui;
    ui::LvglPort& _port;
    char _line[32];
    size_t _len = 0;
};

}  // namespace app
