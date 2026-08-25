// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "structure/java_to_bedrock/JavaBlockEntityToBedrock.h"

#include <cstdint>

#include "mc/deps/nbt/ByteTag.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/IntTag.h"

namespace lholo::structure::detail {
namespace {

std::string joinSignLines(JavaSignSideData const& side) {
    std::string result;
    for (std::size_t line = 0; line < side.messages.size(); ++line) {
        if (line != 0) result.push_back('\n');
        result += javaTextComponentToPlainText(side.messages[line]);
    }
    return result;
}

CompoundTag makeBedrockSignSide(JavaSignSideData const& side) {
    CompoundTag result;
    auto const text = joinSignLines(side);
    // Litematic files normally omit a separately filtered message. Supplying
    // the same safe plain text for both Bedrock fields prevents clients with
    // profanity filtering enabled from selecting an empty alternative.
    result.mTags.insert_or_assign("FilteredText", text);
    result.mTags.insert_or_assign("HideGlowOutline", ByteTag{0});
    result.mTags.insert_or_assign("IgnoreLighting", ByteTag{side.glowing ? 1 : 0});
    result.mTags.insert_or_assign("PersistFormatting", ByteTag{1});
    // The current Bedrock export confirms -16777216 as the default black sign
    // text color. Other Java dye colors need their own verified mapping sample.
    result.mTags.insert_or_assign("SignTextColor", IntTag{static_cast<std::int32_t>(0xff000000u)});
    result.mTags.insert_or_assign("Text", text);
    result.mTags.insert_or_assign("TextOwner", "");
    return result;
}

} // namespace

std::shared_ptr<CompoundTag const> convertJavaSignBlockEntity(
    JavaSignBlockEntityData const& data,
    JavaSignBlockEntityKind        kind,
    int                            x,
    int                            y,
    int                            z
) {
    CompoundTag result;
    result.mTags.insert_or_assign("BackText", makeBedrockSignSide(data.back));
    result.mTags.insert_or_assign("FrontText", makeBedrockSignSide(data.front));
    result.mTags.insert_or_assign(
        "id", std::string{kind == JavaSignBlockEntityKind::HangingSign ? "HangingSign" : "Sign"}
    );
    result.mTags.insert_or_assign("isMovable", ByteTag{1});
    result.mTags.insert_or_assign("IsWaxed", ByteTag{data.waxed ? 1 : 0});
    result.mTags.insert_or_assign("x", IntTag{x});
    result.mTags.insert_or_assign("y", IntTag{y});
    result.mTags.insert_or_assign("z", IntTag{z});
    return std::make_shared<CompoundTag const>(std::move(result));
}

} // namespace lholo::structure::detail
