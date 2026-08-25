// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <array>
#include <memory>
#include <string>

class CompoundTag;

namespace lholo::structure::detail {

struct JavaSignSideData {
    std::array<std::string, 4> messages{};
    bool                       glowing{};
};

struct JavaSignBlockEntityData {
    JavaSignSideData front;
    JavaSignSideData back;
    bool             waxed{};
};

enum class JavaSignBlockEntityKind {
    Sign,
    HangingSign,
};

// Java sign lines are JSON text components, even when they only contain a
// quoted string. Convert the component to the plain text Bedrock signs render.
std::string javaTextComponentToPlainText(std::string const& component);

// Produce the native Bedrock block-entity payload consumed by the existing
// RenderBlock::blockEntityNbt -> BlockActor::load projection path.
std::shared_ptr<CompoundTag const> convertJavaSignBlockEntity(
    JavaSignBlockEntityData const& data,
    JavaSignBlockEntityKind        kind,
    int                            x,
    int                            y,
    int                            z
);

} // namespace lholo::structure::detail
