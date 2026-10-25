-- Build hooks only; resource format and validation stay in Python.
function prepare()
    import("core.project.config")
    local hbui = os.getenv("LHOLO_OREUI_HBUI") or config.get("oreui_hbui")
    if not hbui or #hbui == 0 then
        hbui = path.join(os.projectdir(), "build", "oreui", "runtime")
        os.execv("python", {"tools/oreui/prepare_runtime.py", "--output", hbui})
    end
    hbui = path.absolute(hbui)
    assert(os.isfile(path.join(hbui, "gameplay.html")), "Missing gameplay.html in " .. hbui)
    assert(config.get("target_type") == "client", "LHolo requires target_type=client.")
    if not os.isfile(path.join(os.projectdir(), "frontend/node_modules/esbuild/package.json")) then
        os.execv("npm.cmd", {"--prefix", "frontend", "ci"})
    end
    os.execv("python", {"tools/oreui/build_menu.py", hbui})
    os.execv("node", {"--check", "build/oreui/lholo-menu.js"})
end

function assemble()
    os.execv("python", {"tools/oreui/assemble_package.py"})
end
