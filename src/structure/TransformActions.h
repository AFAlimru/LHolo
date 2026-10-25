#pragma once
#include "structure/TransformTypes.h"

namespace lholo::structure {
bool submitTransformEdit(TransformEdit const& edit);
TransformState getTransformState();
bool processTransformEdits(); // returns whether settings need saving
void resetTransformEdits();
}
