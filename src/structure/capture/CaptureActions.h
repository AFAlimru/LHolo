#pragma once

#include "structure/capture/StructureCapture.h"

namespace lholo::structure::capture {
enum class Action { Apply, FirstFromPlayer, SecondFromPlayer, Clear, Export };
struct Request {
    Action action{};
    Draft draft;
    std::uint64_t revision{};
    std::uint64_t id{};
    std::filesystem::path output;
};
// A single application-frame command. No UI/protocol dependencies.
bool submitAction(Request request);
void processActions();
void resetActions();
bool actionPending();
std::uint64_t completedAction();
}
