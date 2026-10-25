#include "oreui/TransformProtocol.h"
#include "structure/TransformActions.h"
#include "structure/StructureSession.h"
#include "nlohmann/json.hpp"
#include <cstdio>

int runTransformActionsTests() {
    using namespace lholo::structure;
    auto& session = detail::StructureSession::getInstance();
    int failures{};
    auto check = [&](bool ok, char const* message) {
        if (!ok) { ++failures; std::fprintf(stderr, "TransformActionsTests: %s\n", message); }
    };
    resetTransformEdits(); session.resetTransform();
    auto context = getTransformState().context;
    nlohmann::json j{{"id", 100}, {"context", context}, {"field", "x"}, {"expected", 0}, {"value", -29}};
    auto parse = [&] { return lholo::oreui::parseTransformIntent(j.dump()); };
    check(parse().has_value(), "valid signed offset");
    j["value"] = 18446744073709551615ULL; check(!parse(), "uint64 cannot wrap");
    j["value"] = -2147483649LL; check(!parse(), "negative overflow");
    j["value"] = true; check(!parse(), "boolean rejected");
    j["value"] = 1.5; check(!parse(), "fraction rejected");
    j["value"] = "1"; check(!parse(), "string rejected");
    j["field"] = "rotation"; j["value"] = 4; check(!parse(), "rotation bounds");
    j["field"] = "mirror"; j["value"] = 3; check(!parse(), "mirror bounds");
    j["field"] = "unknown"; j["value"] = 0; check(!parse(), "unknown field");
    j["field"] = "x"; j["id"] = 9007199254740992ULL; check(!parse(), "unsafe id");
    check(!lholo::oreui::parseTransformIntent("{") && !lholo::oreui::parseTransformIntent(std::string(4097, ' ')), "malformed and oversized payloads");
    TransformEdit e{100, context, TransformField::X, 0, -29};
    check(submitTransformEdit(e), "queue edit");
    check(session.transform().offsetX == 0, "enqueue does not mutate session");
    check(!submitTransformEdit(e), "concurrent edit rejected");
    check(processTransformEdits(), "control frame requests persistence");
    auto s = getTransformState();
    check(s.ack == 100 && s.accepted && s.values.x == -29, "applied snapshot acknowledged");
    check(!submitTransformEdit(e), "duplicate id rejected");
    e.id++; e.field = TransformField::Rotation; e.value = 3;
    submitTransformEdit(e); check(processTransformEdits(), "rotation applied");
    e.id++; e.field = TransformField::Mirror; e.value = 2;
    submitTransformEdit(e); check(processTransformEdits(), "mirror applied");
    e.id++; e.field = TransformField::Y; e.value = -2147483647 - 1;
    submitTransformEdit(e); check(processTransformEdits(), "minimum offset applied");
    e.id++; e.field = TransformField::Z; e.value = 2147483647;
    submitTransformEdit(e); check(processTransformEdits(), "maximum offset applied");
    e.id++; e.field = TransformField::X; e.expected = -29; e.value = 42;
    submitTransformEdit(e); session.setOffsetX(12);
    check(!processTransformEdits() && !getTransformState().accepted && session.transform().offsetX == 12, "old UI or hotkey wins conflicting edit");
    e.id++; e.expected = 12;
    submitTransformEdit(e); (void)session.beginFileLoad({});
    check(!processTransformEdits() && session.transform().offsetX == 12, "new load rejects stale context");
    e.id++; e.context = getTransformState().context;
    submitTransformEdit(e); resetTransformEdits();
    check(!processTransformEdits() && !getTransformState().accepted, "menu close cancels queued edit");
    session.resetTransform();
    std::printf("TransformActionsTests: %d failures\n", failures);
    return failures;
}
