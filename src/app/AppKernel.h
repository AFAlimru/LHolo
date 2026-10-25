// LHolo - Client-side projection renderer for Minecraft Bedrock Windows
// Copyright (C) 2026  MarmieQi
//
// Single application entry point. The plugin facade only delegates here;
// AppKernel owns enable/disable/load and application control-frame orchestration.

#pragma once

namespace lholo::app {

class AppKernel {
public:
    static AppKernel& getInstance();

    AppKernel(AppKernel const&) = delete;
    AppKernel& operator=(AppKernel const&) = delete;

    bool load();
    bool enable();
    bool disable();
    void processControlFrame();

private:
    AppKernel() = default;
};

} // namespace lholo::app
