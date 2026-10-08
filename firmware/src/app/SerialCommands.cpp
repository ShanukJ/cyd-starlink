#include "SerialCommands.h"

#include <Arduino.h>

#include <memory>

#include "../utils/Log.h"

namespace app {

void SerialCommands::poll() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c == '\r') continue;
        if (c != '\n') {
            if (_len < sizeof(_line) - 1) _line[_len++] = static_cast<char>(c);
            continue;
        }
        _line[_len] = '\0';
        _len = 0;
        if (strcmp(_line, "screenshot") == 0) {
            screenshot();
        } else if (strncmp(_line, "page ", 5) == 0) {
            _ui.showPage(atoi(_line + 5));
        } else if (strncmp(_line, "rotate ", 7) == 0) {
            _port.setRotation(static_cast<uint8_t>(atoi(_line + 7)) & 3);
        } else if (_line[0]) {
            LOG("CMD", "Unknown command \"%s\" (try: screenshot, page <n>, rotate <r>)", _line);
        }
    }
}

// Protocol: "#SCREENSHOT <w> <h>\n", then h rows of w little-endian RGB565
// pixels (raw binary), then "\n#END\n".
void SerialCommands::screenshot() {
    hw::DisplayDriver& d = _board.display();
    const int32_t w = d.width(), h = d.height();
    std::unique_ptr<uint16_t[]> row(new (std::nothrow) uint16_t[w]);
    if (!row || !d.readPixels(0, 0, 1, 1, row.get())) {
        LOG("CMD", "Screenshot not supported on this display");
        return;
    }
    Serial.printf("\n#SCREENSHOT %ld %ld\n", (long)w, (long)h);
    for (int32_t y = 0; y < h; ++y) {
        d.readPixels(0, y, w, 1, row.get());
        Serial.write(reinterpret_cast<const uint8_t*>(row.get()), w * sizeof(uint16_t));
    }
    Serial.print("\n#END\n");
}

}  // namespace app
