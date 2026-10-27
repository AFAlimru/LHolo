#include "app/RenderSettingsActions.h"
#include "oreui/RenderSettingsProtocol.h"
#include "structure/StructureSession.h"
#include "projection/Projection.h"
#include "nlohmann/json.hpp"
#include <cstdio>

// Standalone tests replace only the projection setting boundary, not the
// production action runner, session, protocol or layer clamping rules.
namespace lholo::projection {
namespace { float fill = .15f, outline = 1; bool bounds = true, correction{}, missing{}; }
float getCorrectionFillOpacity() { return fill; }
void setCorrectionFillOpacity(float v) { fill = v; }
float getCorrectionOutlineOpacity() { return outline; }
void setCorrectionOutlineOpacity(float v) { outline = v; }
bool getStructureBoundsEnabled() { return bounds; }
void setStructureBoundsEnabled(bool v) { bounds = v; }
bool getCorrectionSeeThrough() { return correction; }
void setCorrectionSeeThrough(bool v) { correction = v; }
bool getMissingSeeThrough() { return missing; }
void setMissingSeeThrough(bool v) { missing = v; }
}

int runRenderSettingsTests() {
    using namespace lholo;
    using namespace app;
    auto& session = structure::detail::StructureSession::getInstance();
    int failures{};
    auto check = [&](bool ok, char const* message) {
        if (!ok) { ++failures; std::fprintf(stderr, "RenderSettingsTests: %s\n", message); }
    };
    session.clearLoaded({}); session.resetTransform(); resetRenderSettingsEdits();
    auto loaded = std::make_shared<structure::LoadedStructure>();
    loaded->sizeX = 9; loaded->sizeY = 21; loaded->materialCount = 3;
    auto load = session.beginFileLoad({});
    check(session.completeFileLoad(load, loaded, "render-test.mcstructure", {}), "test structure activated");
    auto state = getRenderSettingsState();
    check(state.maximum == std::array{20, 8, 2}, "actual Y/X/material bounds");
    nlohmann::json j{{"id", 1}, {"context", state.context}, {"axis", 0},
        {"changes", {{{"field", "fill"}, {"expected", 15}, {"value", 42}}}}};
    auto parse = [&] { return oreui::parseRenderSettingsIntent(j.dump()); };
    check(parse().has_value(), "valid percentage edit");
    auto invalid = j;
    invalid["changes"][0]["value"] = 101; check(!oreui::parseRenderSettingsIntent(invalid.dump()), "percentage bounds");
    invalid["changes"][0]["value"] = true; check(!oreui::parseRenderSettingsIntent(invalid.dump()), "bool not numeric");
    invalid["changes"][0]["value"] = 1.5; check(!oreui::parseRenderSettingsIntent(invalid.dump()), "fraction rejected");
    invalid["changes"][0]["value"] = 18446744073709551615ULL; check(!oreui::parseRenderSettingsIntent(invalid.dump()), "uint64 cannot wrap");
    invalid = j; invalid["changes"][0]["field"] = "bounds";
    check(!oreui::parseRenderSettingsIntent(invalid.dump()), "number not boolean");
    invalid = j; invalid["changes"].push_back(invalid["changes"][0]);
    check(!oreui::parseRenderSettingsIntent(invalid.dump()), "duplicate field rejected");
    invalid = j; invalid["axis"] = 3; check(!oreui::parseRenderSettingsIntent(invalid.dump()), "axis bounds");
    invalid = j; invalid["id"] = 9007199254740992ULL; check(!oreui::parseRenderSettingsIntent(invalid.dump()), "unsafe id rejected");
    check(!oreui::parseRenderSettingsIntent("{") && !oreui::parseRenderSettingsIntent(std::string(4097, ' ')), "malformed and oversized");
    auto edit = *parse();
    check(submitRenderSettingsEdit(edit), "queued edit");
    check(getRenderSettingsState().values.fill == 15, "enqueue cannot mutate projection");
    check(!submitRenderSettingsEdit(edit), "concurrent and duplicate rejected");
    check(processRenderSettingsEdits(), "control frame requests save");
    state = getRenderSettingsState();
    check(state.ack == 1 && state.accepted && state.values.fill == 42, "percentage applies and acknowledges");
    check(!submitRenderSettingsEdit(edit), "processed duplicate rejected");
    auto apply = [&](std::vector<RenderFieldEdit> changes, structure::LayerAxis axis = structure::LayerAxis::Y) {
        edit = {edit.id + 1, getRenderSettingsState().context, axis, std::move(changes)};
        check(submitRenderSettingsEdit(edit), "queue next edit");
        return processRenderSettingsEdits();
    };
    check(apply({{RenderField::Bounds, true, false}, {RenderField::CorrectionSeeThrough, false, true},
        {RenderField::MissingSeeThrough, false, true}}), "visual switches applied");
    check(apply({{RenderField::Layer, 0, 19}, {RenderField::Mode, 0, 1}}), "Y layer and range");
    check(apply({{RenderField::Axis, 0, 1}}) && getRenderSettingsState().values.layers.layer == 8, "axis clamps to X bound");
    check(apply({{RenderField::Axis, 1, 2}}, structure::LayerAxis::X) && getRenderSettingsState().values.layers.layer == 2, "axis clamps to material bound");
    auto oldFill = projection::getCorrectionFillOpacity();
    check(!apply({{RenderField::Layer, 2, 1}, {RenderField::Fill, 42, 60}}, structure::LayerAxis::Y)
        && projection::getCorrectionFillOpacity() == oldFill, "wrong axis rejects entire batch");
    check(!apply({{RenderField::Fill, 41, 15}, {RenderField::Outline, 100, 20}})
        && getRenderSettingsState().values.outline == 100, "conflicting field rejects entire style batch");
    check(apply({{RenderField::Fill, 42, 15}, {RenderField::Outline, 100, 100}}), "default style as one batch");
    edit = {edit.id + 1, getRenderSettingsState().context, structure::LayerAxis::Material, {{RenderField::Layer, 2, 1}}};
    submitRenderSettingsEdit(edit); session.setDisplayLayer(0);
    check(!processRenderSettingsEdits() && session.transform().displayLayer == 0, "hotkey wins stale expected layer");
    edit.id++; edit.changes = {{RenderField::Fill, 15, 50}};
    submitRenderSettingsEdit(edit); (void)session.beginFileLoad({});
    check(!processRenderSettingsEdits() && getRenderSettingsState().values.fill == 15, "new load invalidates queued settings");
    edit.id++; edit.context = getRenderSettingsState().context;
    submitRenderSettingsEdit(edit); resetRenderSettingsEdits();
    check(!processRenderSettingsEdits() && !getRenderSettingsState().accepted, "menu close cancels pending edit");
    auto composed = std::make_shared<structure::LoadedStructure>();
    composed->composed = true;
    auto request = session.beginCompositionLoad({});
    session.completeCompositionLoad(request, composed, {});
    auto editor = std::make_shared<structure::LoadedStructure>();
    editor->sizeY = 10;
    session.selectCompositionFile(editor, {}, 0, "task-file.mcstructure");
    check(!apply({{RenderField::Layer, 0, 2}, {RenderField::Mode, 0, 1}}) &&
          getRenderSettingsState().accepted && session.transform().displayLayer == 2,
          "task layer change applies without requesting a global config save");
    check(apply({{RenderField::Fill, 15, 20}}), "task visual style still requests global persistence");
    session.clearLoaded({}); session.resetTransform();
    check(getRenderSettingsState().maximum == std::array{0, 0, 0}, "empty session bounds");
    std::printf("RenderSettingsTests: %d failures\n", failures);
    return failures;
}
