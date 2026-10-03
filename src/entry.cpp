// v15: fixed queen rescue, certified death relay, reserved pearl recovery.
#include "controller.hpp"

int main() {
    // Static storage avoids placing the world-memory arrays on the WASM stack.
    static battlecode::Bot bot;
    return bot.run();
}
