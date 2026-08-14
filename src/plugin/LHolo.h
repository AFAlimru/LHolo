#pragma once

namespace ll::mod {
class NativeMod;
}

namespace lholo {

class LHolo {
public:
    static LHolo& getInstance();

    LHolo(LHolo const&)            = delete;
    LHolo(LHolo&&)                 = delete;
    LHolo& operator=(LHolo const&) = delete;
    LHolo& operator=(LHolo&&)      = delete;

    bool load();
    bool enable();
    bool disable();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const;

private:
    LHolo();

    ll::mod::NativeMod& mSelf;
};

} // namespace lholo
