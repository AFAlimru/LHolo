// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Structure session state owned by the structure module: loaded structure,
// transform/layer values, restore snapshot and status strings. Accessors
// return the underlying storage so existing logic keeps its exact shapes.

#pragma once

#include "structure/StructureLoader.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace lholo::structure::detail {

std::mutex&                       sessionLoadedMutex();
std::shared_ptr<LoadedStructure>& sessionLoaded();

std::atomic_int& sessionRotationQuarterTurns();
std::atomic_int& sessionMirror();
std::atomic_int& sessionOffsetX();
std::atomic_int& sessionOffsetY();
std::atomic_int& sessionOffsetZ();
std::atomic_int& sessionLayerDisplayMode();
std::atomic_int& sessionDisplayLayer();
std::atomic_int& sessionLayerAxis();

std::atomic_bool& sessionHasSavedProjection();
std::atomic_int&  sessionSavedAnchorX();
std::atomic_int&  sessionSavedAnchorY();
std::atomic_int&  sessionSavedAnchorZ();
std::atomic_int&  sessionSavedRotation();
std::atomic_int&  sessionSavedMirror();
std::atomic_int&  sessionSavedOffsetX();
std::atomic_int&  sessionSavedOffsetY();
std::atomic_int&  sessionSavedOffsetZ();
std::atomic_int&  sessionSavedLayerDisplayMode();
std::atomic_int&  sessionSavedDisplayLayer();
std::atomic_int&  sessionSavedLayerAxis();
std::string&      sessionSavedStructurePath();

std::string& sessionLastPath();
std::string& sessionStatus();

int maxLayerFor(LoadedStructure const& structure, int axis);

} // namespace lholo::structure::detail
