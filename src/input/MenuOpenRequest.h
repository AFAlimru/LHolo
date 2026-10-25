#pragma once

namespace lholo::input {
// Input only queues intent; the application chooses the menu implementation.
void requestMenuOpen();
bool consumeMenuOpenRequest();
void clearMenuOpenRequest();
}
