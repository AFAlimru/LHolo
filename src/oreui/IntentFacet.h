#pragma once

#include "mc/client/gui/oreui/interface/IFacet.h"
#include <memory>
#include <string_view>

namespace lholo::oreui {
// One native facet activation carries one intent. It owns no game objects.
class IntentFacet final : public OreUI::IFacet {
public:
    using Sink = bool (*)(std::string_view);
    explicit IntentFacet(Sink sink, char const* name = "lholo.capture.intent", std::size_t maximumBytes = 4096)
        : mSink(sink), mName(name), mMaximumBytes(maximumBytes) {
        // Generated SDK headers expose no base constructor. The game's base
        // destructor invalidates this block, so default-null is not a valid
        // state for a mod-created EnableNonOwnerReferences instance.
        mControlBlock = std::make_shared<ControlBlock>(ControlBlock{true});
    }
    void unbind(OreUI::FacetBinder&) override {}
    void sync(OreUI::FacetBinder&, std::string const&) override {}
    bool update() override { return false; }
    char const* name() const override { return mName; }
    OreUI::Status init(std::unordered_map<std::string, std::variant<double, bool, std::string>> const& payload) override {
        auto const found = payload.find("request");
        if (found == payload.end()) return OreUI::Status::Error;
        auto const* request = std::get_if<std::string>(&found->second);
        if (!request || request->size() > mMaximumBytes || !mSink) return OreUI::Status::Error;
        return mSink(*request) ? OreUI::Status::Ok : OreUI::Status::Error;
    }
private:
    Sink mSink;
    char const* mName;
    std::size_t mMaximumBytes;
};
}
