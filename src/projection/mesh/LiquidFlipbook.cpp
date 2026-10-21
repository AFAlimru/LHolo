// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi

#include "projection/mesh/LiquidFlipbook.h"

#include "plugin/LHolo.h"

#include "ll/api/mod/NativeMod.h"

#include "mc/client/game/IClientInstance.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/core/image/Image.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/minecraft_renderer/renderer/IsMissingTexture.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/resources/ResourcePackManager.h"
#include "mc/world/level/block/Block.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lholo::projection::detail {
namespace {

constexpr std::uint64_t kTickMs = 50;
constexpr std::uint64_t kSourceCheckMs = 2000;
// Blended flipbooks move toward the next frame in this many steps per frame.
constexpr int kBlendSteps = 8;

// One flipbook image: square frames stacked vertically, as RGBA8.
struct FlipbookSource {
    std::string               path;
    std::uint32_t             frameSize{};
    std::uint32_t             frameCount{};
    std::uint16_t             sourceWidth{};
    std::uint16_t             sourceHeight{};
    std::vector<std::uint8_t> rgba;
    std::vector<int>          sequence;
    int                       ticksPerFrame{1};
    bool                      blend{true};
    mce::ImageUsage           usage{mce::ImageUsage::SRGB};
};

// Two textures take turns: a frame is written into the one not on screen and
// shown from the next update on, so a draw never samples a texture whose
// in-place update is still in flight (moving the camera flickered).
struct FlipbookSlot {
    std::optional<ResourceLocation>       location;
    std::optional<mce::TexturePtr>        texture;
    std::optional<render::TextureVariant> variant;
};

struct FlipbookState {
    std::array<FlipbookSource, 4> sources;
    std::uint32_t                 cellSize{};
    std::array<FlipbookSlot, 2>   slots;
    std::size_t                   shown{};
    std::uint64_t                 lastKey{~0ULL};
    std::uint64_t                 nextSourceCheck{};
};

FlipbookState& flipbook() {
    static FlipbookState state;
    return state;
}

std::atomic_bool sReady{false};

auto& logger() {
    return LHolo::getInstance().getSelf().getLogger();
}

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count());
}

// Texture slot 0 is the still tile and slot 2 the flowing tile (see the
// liquid hull builder).
TextureUVCoordinateSet const* liquidTile(char const* blockName, std::uint64_t slot) {
    auto block = Block::tryGetFromRegistry(HashedString{blockName});
    if (!block) return nullptr;
    auto const* graphics = BlockGraphics::getForBlock(*block);
    return graphics ? &graphics->getTexture(slot, 0) : nullptr;
}

std::array<TextureUVCoordinateSet const*, 4> liquidTiles() {
    return {
        liquidTile("minecraft:water", 0),
        liquidTile("minecraft:water", 2),
        liquidTile("minecraft:lava", 0),
        liquidTile("minecraft:lava", 2),
    };
}

// flipbook_textures.json entries for the source paths: frame timing and order.
// Missing entries keep the defaults (one tick per frame, every frame, blended).
void applyFlipbookSettings(ResourcePackManager& packs, std::array<FlipbookSource, 4>& sources) {
    std::string text;
    if (!packs.load(
            ResourceLocation{Core::PathView{"textures/flipbook_textures.json"}, ResourceFileSystem::UserPackage},
            text
        )) {
        return;
    }
    auto const document = nlohmann::json::parse(text, nullptr, false, true);
    if (!document.is_array()) return;
    for (auto const& entry : document) {
        if (!entry.is_object()) continue;
        auto const texture = entry.find("flipbook_texture");
        if (texture == entry.end() || !texture->is_string()) continue;
        for (auto& source : sources) {
            if (texture->get<std::string>() != source.path) continue;
            if (auto const ticks = entry.find("ticks_per_frame"); ticks != entry.end() && ticks->is_number_integer()) {
                source.ticksPerFrame = std::max(1, ticks->get<int>());
            }
            if (auto const blend = entry.find("blend_frames"); blend != entry.end() && blend->is_boolean()) {
                source.blend = blend->get<bool>();
            }
            if (auto const frames = entry.find("frames"); frames != entry.end() && frames->is_array()) {
                std::vector<int> sequence;
                for (auto const& frame : *frames) {
                    if (!frame.is_number_integer()) continue;
                    int const index = frame.get<int>();
                    if (index >= 0 && index < static_cast<int>(source.frameCount)) sequence.push_back(index);
                }
                if (!sequence.empty()) source.sequence = std::move(sequence);
            }
        }
    }
}

// The tile's image as the active resource packs provide it.
bool loadSource(ResourcePackManager& packs, TextureUVCoordinateSet const& tile, FlipbookSource& source) {
    auto const& location = static_cast<ResourceLocation const&>(tile.sourceFileLocation);
    source = FlipbookSource{};
    source.path = location.mPath.get().value;
    source.sourceWidth = tile._sourceImageWidth;
    source.sourceHeight = tile._sourceImageHeight;
    mce::Image image = packs.loadTexture(location);
    std::uint32_t const width = image.mWidth;
    std::uint32_t const height = image.mHeight;
    int channels = 0;
    if (image.imageFormat == mce::ImageFormat::RGBA8Unorm) channels = 4;
    else if (image.imageFormat == mce::ImageFormat::RGB8Unorm) channels = 3;
    if (channels == 0 || width == 0 || height < width
        || image.mImageBytes.size() < static_cast<std::size_t>(width) * height * channels) {
        logger().warn("Liquid flipbook: unusable image {} ({}x{})", source.path, width, height);
        return false;
    }
    source.frameSize = width;
    source.frameCount = height / width;
    source.usage = image.mUsage;
    source.rgba.resize(static_cast<std::size_t>(width) * height * 4);
    auto const* bytes = image.mImageBytes.data();
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        for (int c = 0; c < 3; ++c) source.rgba[pixel * 4 + c] = bytes[pixel * channels + c];
        source.rgba[pixel * 4 + 3] = channels == 4 ? bytes[pixel * 4 + 3] : 255;
    }
    for (std::uint32_t frame = 0; frame < source.frameCount; ++frame) {
        source.sequence.push_back(static_cast<int>(frame));
    }
    return true;
}

bool loadSources(IClientInstance& client, std::array<FlipbookSource, 4>& sources, std::uint32_t& cellSize) {
    auto& packs = client.getResourcePackManager();
    auto const tiles = liquidTiles();
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        if (!tiles[index] || !loadSource(packs, *tiles[index], sources[index])) return false;
    }
    applyFlipbookSettings(packs, sources);
    cellSize = 0;
    for (auto const& source : sources) cellSize = std::max(cellSize, source.frameSize);
    return cellSize > 0;
}

bool sourcesChanged(FlipbookState const& state) {
    auto const tiles = liquidTiles();
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        if (!tiles[index]) return false;  // registry mid-reload; check again later
        auto const& source = state.sources[index];
        if (tiles[index]->_sourceImageWidth != source.sourceWidth
            || tiles[index]->_sourceImageHeight != source.sourceHeight) {
            return true;
        }
    }
    return false;
}

// Current frame of every source, blended toward the next one when the
// flipbook asks for it, written into its cell (nearest-neighbour scaled).
mce::Image composeFrame(FlipbookState const& state, std::uint64_t now) {
    std::uint32_t const size = state.cellSize * 2;
    mce::Image image(size, size, mce::ImageFormat::RGBA8Unorm, state.sources[0].usage);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
    double const ticks = static_cast<double>(now) / static_cast<double>(kTickMs);
    for (std::size_t cell = 0; cell < state.sources.size(); ++cell) {
        auto const& source = state.sources[cell];
        double const frames = ticks / source.ticksPerFrame;
        auto const step = static_cast<std::uint64_t>(frames);
        auto const count = source.sequence.size();
        int const current = source.sequence[step % count];
        int const next = source.sequence[(step + 1) % count];
        int const mix = source.blend
            ? static_cast<int>((frames - static_cast<double>(step)) * kBlendSteps) * 256 / kBlendSteps
            : 0;
        std::uint32_t const cellX = static_cast<std::uint32_t>(cell % 2) * state.cellSize;
        std::uint32_t const cellY = static_cast<std::uint32_t>(cell / 2) * state.cellSize;
        for (std::uint32_t y = 0; y < state.cellSize; ++y) {
            std::uint32_t const sy = y * source.frameSize / state.cellSize;
            for (std::uint32_t x = 0; x < state.cellSize; ++x) {
                std::uint32_t const sx = x * source.frameSize / state.cellSize;
                auto const offsetOf = [&](int frame) {
                    return ((static_cast<std::size_t>(frame) * source.frameSize + sy) * source.frameSize + sx) * 4;
                };
                auto const* a = &source.rgba[offsetOf(current)];
                auto const* b = &source.rgba[offsetOf(next)];
                auto* out = &pixels[((static_cast<std::size_t>(cellY + y) * size) + cellX + x) * 4];
                for (int c = 0; c < 4; ++c) out[c] = static_cast<std::uint8_t>((a[c] * (256 - mix) + b[c] * mix) >> 8);
            }
        }
    }
    image.mImageBytes = mce::Blob{pixels.data(), pixels.size()};
    return image;
}

std::uint64_t animationKey(FlipbookState const& state, std::uint64_t now) {
    // Changes whenever any cell's frame or blend step changes.
    std::uint64_t key = 0;
    double const ticks = static_cast<double>(now) / static_cast<double>(kTickMs);
    for (auto const& source : state.sources) {
        double const frames = ticks / source.ticksPerFrame;
        auto const step = static_cast<std::uint64_t>(frames);
        auto const blendStep = source.blend ? static_cast<std::uint64_t>((frames - static_cast<double>(step)) * kBlendSteps) : 0;
        key = key * 1000003ULL + (step % source.sequence.size()) * kBlendSteps + blendStep;
    }
    return key;
}

bool texturesAlive(FlipbookState const& state) {
    for (auto const& slot : state.slots) {
        if (!slot.texture) return false;
        auto const& data = slot.texture->mClientTexture.get();
        if (!data || data->mIsMissingTexture != IsMissingTexture::No) return false;
    }
    return true;
}

bool upload(IClientInstance& client, FlipbookState& state, std::uint64_t now) {
    auto group = client.getTextureGroup();
    if (!group) return false;
    auto const image = composeFrame(state, now);
    for (std::size_t index = 0; index < state.slots.size(); ++index) {
        auto& slot = state.slots[index];
        if (!slot.location) {
            slot.location.emplace(
                Core::PathView{index == 0 ? "lholo/liquid_flipbook_a" : "lholo/liquid_flipbook_b"},
                ResourceFileSystem::Raw
            );
        }
        if (slot.texture) group->unloadTexture(*slot.location, false);
        slot.variant.reset();
        slot.texture.reset();
        group->uploadTexture(*slot.location, cg::ImageBuffer{image});
        slot.texture.emplace(group, *slot.location);
        slot.variant.emplace(*slot.texture);
    }
    state.shown = 0;
    state.lastKey = animationKey(state, now);
    return true;
}

} // namespace

LiquidFlipbookUv liquidFlipbookCellUv(LiquidFlipbookCell cell) {
    auto const index = static_cast<int>(cell);
    float const u0 = static_cast<float>(index % 2) * 0.5f;
    float const v0 = static_cast<float>(index / 2) * 0.5f;
    return {u0, v0, u0 + 0.5f, v0 + 0.5f};
}

bool liquidFlipbookReady() {
    return sReady.load(std::memory_order_acquire);
}

bool prepareLiquidFlipbook(IClientInstance& client) {
    if (liquidFlipbookReady()) return true;
    auto& state = flipbook();
    auto const now = nowMs();
    if (!loadSources(client, state.sources, state.cellSize) || !upload(client, state, now)) return false;
    state.nextSourceCheck = now + kSourceCheckMs;
    sReady.store(true, std::memory_order_release);
    return true;
}

render::TextureVariant const* acquireLiquidFlipbookTexture(IClientInstance& client) {
    if (!liquidFlipbookReady()) return nullptr;
    auto& state = flipbook();
    auto const now = nowMs();
    // A resource pack change replaces the flipbook images (and can drop the
    // uploaded textures); rebuild from the new packs. On failure the last
    // frames stay, so meshes built for the flipbook still draw.
    bool reupload = !texturesAlive(state);
    if (now >= state.nextSourceCheck) {
        state.nextSourceCheck = now + kSourceCheckMs;
        if (sourcesChanged(state)) {
            std::array<FlipbookSource, 4> sources;
            std::uint32_t                 cellSize{};
            if (loadSources(client, sources, cellSize)) {
                state.sources = std::move(sources);
                state.cellSize = cellSize;
                reupload = true;
            }
        }
    }
    if (reupload) {
        upload(client, state, now);
    } else if (auto const key = animationKey(state, now); key != state.lastKey) {
        if (auto group = client.getTextureGroup()) {
            // Show the frame written last time; write this one off screen.
            std::size_t const hidden = state.shown;
            state.shown = 1 - state.shown;
            group->updateTextureInPlace(*state.slots[hidden].location, cg::ImageBuffer{composeFrame(state, now)});
            state.lastKey = key;
        }
    }
    auto const& variant = state.slots[state.shown].variant;
    return variant ? &*variant : nullptr;
}

} // namespace lholo::projection::detail
