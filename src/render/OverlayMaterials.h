// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Shared engine resources for correction and box overlays. Resource lookup
// does not own projection/capture state or cache world-specific material handles.

#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/deps/core/renderer/RenderMaterialInfo.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/game/LevelRenderer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core_graphics/TextureSetLayerType.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/IsMissingTexture.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"

namespace lholo::render {

using TextureVariant = std::variant<std::monostate, mce::TexturePtr, mce::ClientTexture, mce::ServerTexture>;

inline bool materialExists(mce::MaterialPtr const& material) {
    return material.mRenderMaterialInfoPtr.get() != nullptr;
}

inline mce::MaterialPtr const& selectionOutlineMaterial(LevelRenderer& renderer) {
    return renderer.mLevelRendererPlayer->mOutlineSelectionMaterial.get();
}

// The vanilla 2x2 pure-white texture. Sampled at its center it reads exactly
// (1,1,1,1): the texture multiply stays neutral and alpha tests never discard.
// Resolved once from the vanilla TextureGroup, retried per frame until it
// shows up, and left empty when unavailable.
inline TextureVariant resolveWhiteTextureVariant(LevelRenderer& levelRenderer) {
    static mce::TexturePtr cached{};
    static bool            resolved = false;
    if (!resolved) {
        auto const& textureGroup = levelRenderer.mTextureGroup.get();
        if (textureGroup) {
            auto texture = textureGroup->getTexture(
                ResourceLocation{Core::PathView{"textures/ui/white_background"}},
                false,
                std::nullopt,
                cg::TextureSetLayerType::Color
            );
            auto const& clientTexture = texture.mClientTexture;
            if (clientTexture && clientTexture->mIsMissingTexture != IsMissingTexture::Yes) {
                cached   = std::move(texture);
                resolved = true;
            }
        }
    }
    if (resolved) return TextureVariant{cached};
    return TextureVariant{};
}

// The beacon beam's translucent material: alpha blended, no depth writes, and
// it samples a plain 2D texture (the block materials expect the terrain atlas
// texture set and show the missing texture for anything else) modulated by
// COLOR0. Resolved once from the runtime material groups; null when absent.
//
// The client library exports no mce::MaterialPtr constructor, so the resolved
// handle lives in zero-initialized aligned storage and only the internal
// shared pointer is ever assigned. The retained reference keeps the material
// alive across resource reloads.
inline mce::MaterialPtr const* resolveBeaconBeamTransparentMaterial() {
    alignas(mce::MaterialPtr) static std::byte storage[sizeof(mce::MaterialPtr)]{};
    static auto* const                        cached   = reinterpret_cast<mce::MaterialPtr*>(storage);
    static bool                               resolved = false;
    if (!resolved) {
        resolved = true;
        bool found = false;
        auto const scan = [&](mce::RenderMaterialGroup& group) {
            if (found) return;
            for (auto const& entry : group.mMaterials.get()) {
                auto const& info = entry.second;
                if (!info || !info->mPtr) continue;
                if (entry.first.getString() != "beacon_beam_transparent") continue;
                cached->mRenderMaterialInfoPtr = info;
                found                          = true;
                break;
            }
        };
        scan(mce::RenderMaterialGroup::switchable());
        if (!found) scan(mce::RenderMaterialGroup::common());
    }
    return materialExists(*cached) ? cached : nullptr;
}

} // namespace lholo::render
