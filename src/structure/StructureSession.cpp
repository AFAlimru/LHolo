// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "structure/StructureSession.h"

#include <algorithm>
#include <atomic>

namespace lholo::structure::detail {
namespace {

std::mutex                       gLoadedMutex;
std::shared_ptr<LoadedStructure> gLoaded;
std::atomic_int                  gRotationQuarterTurns{0};
std::atomic_int                  gMirrorMode{0};
std::atomic_int                  gOffsetX{0};
std::atomic_int                  gOffsetY{0};
std::atomic_int                  gOffsetZ{0};
std::atomic_int                  gLayerDisplayMode{0};
std::atomic_int                  gDisplayLayer{0};
std::atomic_int                  gLayerAxis{0};
std::atomic_bool                 gHasSavedProjection{false};
std::atomic_int                  gSavedAnchorX{0};
std::atomic_int                  gSavedAnchorY{0};
std::atomic_int                  gSavedAnchorZ{0};
std::atomic_int                  gSavedRotation{0};
std::atomic_int                  gSavedMirror{0};
std::atomic_int                  gSavedOffsetX{0};
std::atomic_int                  gSavedOffsetY{0};
std::atomic_int                  gSavedOffsetZ{0};
std::atomic_int                  gSavedLayerDisplayMode{0};
std::atomic_int                  gSavedDisplayLayer{0};
std::atomic_int                  gSavedLayerAxis{0};
std::string                      gSavedStructurePath;
std::string                      gLastPath;
std::string                      gStatus = "尚未加载结构文件";

} // namespace

std::mutex& sessionLoadedMutex() { return gLoadedMutex; }
std::shared_ptr<LoadedStructure>& sessionLoaded() { return gLoaded; }

std::atomic_int& sessionRotationQuarterTurns() { return gRotationQuarterTurns; }
std::atomic_int& sessionMirror() { return gMirrorMode; }
std::atomic_int& sessionOffsetX() { return gOffsetX; }
std::atomic_int& sessionOffsetY() { return gOffsetY; }
std::atomic_int& sessionOffsetZ() { return gOffsetZ; }
std::atomic_int& sessionLayerDisplayMode() { return gLayerDisplayMode; }
std::atomic_int& sessionDisplayLayer() { return gDisplayLayer; }
std::atomic_int& sessionLayerAxis() { return gLayerAxis; }

std::atomic_bool& sessionHasSavedProjection() { return gHasSavedProjection; }
std::atomic_int& sessionSavedAnchorX() { return gSavedAnchorX; }
std::atomic_int& sessionSavedAnchorY() { return gSavedAnchorY; }
std::atomic_int& sessionSavedAnchorZ() { return gSavedAnchorZ; }
std::atomic_int& sessionSavedRotation() { return gSavedRotation; }
std::atomic_int& sessionSavedMirror() { return gSavedMirror; }
std::atomic_int& sessionSavedOffsetX() { return gSavedOffsetX; }
std::atomic_int& sessionSavedOffsetY() { return gSavedOffsetY; }
std::atomic_int& sessionSavedOffsetZ() { return gSavedOffsetZ; }
std::atomic_int& sessionSavedLayerDisplayMode() { return gSavedLayerDisplayMode; }
std::atomic_int& sessionSavedDisplayLayer() { return gSavedDisplayLayer; }
std::atomic_int& sessionSavedLayerAxis() { return gSavedLayerAxis; }
std::string& sessionSavedStructurePath() { return gSavedStructurePath; }

std::string& sessionLastPath() { return gLastPath; }
std::string& sessionStatus() { return gStatus; }

int maxLayerFor(LoadedStructure const& structure, int axis) {
    return std::max(0, (axis == 1 ? structure.sizeX : structure.sizeY) - 1);
}

} // namespace lholo::structure::detail
