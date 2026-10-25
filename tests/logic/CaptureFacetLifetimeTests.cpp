#include "oreui/IntentFacet.h"
#include <cstdio>
#include <string>

namespace {
int missingControlBlocks{};
int destructions{};
bool accept{};
std::string received;
bool receive(std::string_view value) { received = value; return accept; }
}

// The standalone target has no Minecraft runtime. Model its non-owner
// invalidation boundary; inspect the *production* facet's actual base field,
// instead of replacing the facet with a frontend mock.
Bedrock::EnableNonOwnerReferences::~EnableNonOwnerReferences() {
    ++destructions;
    if (!mControlBlock) ++missingControlBlocks;
    else mControlBlock->mIsValid = false;
}

int runCaptureFacetLifetimeTests() {
    using lholo::oreui::IntentFacet;
    using Payload = std::unordered_map<std::string, std::variant<double, bool, std::string>>;
    int failures{};
    auto check = [&](bool value, char const* name) {
        if (!value) { ++failures; std::fprintf(stderr, "CaptureFacetLifetimeTests: %s\n", name); }
    };
    auto const before = destructions;
    Payload const payload{{"request", std::string{"{\"action\":\"first\"}"}}};
    for (int index = 0; index < 32; ++index) {
        std::shared_ptr<Bedrock::EnableNonOwnerReferences::ControlBlock> observer;
        {
            auto const* name = index % 2 ? "lholo.capture.intent" : "lholo.transform.intent";
            std::unique_ptr<OreUI::IFacet> facet = std::make_unique<IntentFacet>(receive, name);
            check(std::string_view(facet->name()) == name, "each intent exposes its registered name");
            observer = facet->mControlBlock;
            check(observer && observer->mIsValid, "construct valid non-owner control block");
            accept = true;
            check(facet->init(payload) == OreUI::Status::Ok, "activate repeated intent");
            check(received == "{\"action\":\"first\"}", "payload reaches sink unchanged");
        } // facet:discard destroys through IFacet, including its native base.
        check(observer && !observer->mIsValid, "discard invalidates observers");
        std::weak_ptr weak = observer;
        observer.reset();
        check(weak.expired(), "discard releases ownership without leaking");
    }
    {
        auto facet = std::make_unique<IntentFacet>(receive);
        accept = false;
        check(facet->init(payload) == OreUI::Status::Error, "rejected first request uses safe cleanup");
    }
    {
        auto facet = std::make_unique<IntentFacet>(receive);
        check(facet->init({}) == OreUI::Status::Error, "missing payload uses safe cleanup");
        check(facet->init(Payload{{"request", true}}) == OreUI::Status::Error, "wrong type rejected");
    }
    check(missingControlBlocks == 0, "all destruction paths have initialized control blocks");
    check(destructions - before == 34, "all facets destroyed");
    std::printf("CaptureFacetLifetimeTests: %d failures\n", failures);
    return failures;
}
