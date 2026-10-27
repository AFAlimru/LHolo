// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#pragma once

#include <string>
#include "structure/TaskComposer.h"
#include "structure/StructureSession.h"

namespace lholo::structure::detail {

struct StructureLoadOrigin;

// One owned background task; normal close/world exit invalidates publication
// without waiting. Level destruction and mod shutdown wait for file parsing
// before the world's registry or this DLL can be released.
void enableStructureFileLoader();
void shutdownStructureFileLoader();
bool isStructureFileLoading();
// Advances on Level destruction or loader restart, even while tasks are closed.
std::uint64_t structureFileWorldRevision();
bool startStructureFileLoad(std::string path, StructureLoadOrigin origin);
bool startSavedProjectionLoad();
struct CompositionLoadResult {
    StructureLoadRequest request;
    std::uint64_t revision{};
    std::string taskId, error, errorFileId;
    std::vector<CompositionFile> files;
    Composition composition;
};
bool startTaskComposition(std::string taskId, std::uint64_t revision, StructureLoadOrigin,
    std::vector<CompositionFile>, std::vector<std::string> paths, bool reloadFiles);
std::shared_ptr<CompositionLoadResult> takeTaskCompositionResult();
std::shared_ptr<LoadedStructure const> cachedTaskFile(std::string const& path);

} // namespace lholo::structure::detail
