param(
    [string]$GameRoot = 'D:\games\LeviLauncher\MC\versions\1.26.51.01'
)
$ErrorActionPreference = 'Stop'
function Get-LHoloFileHash([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '') }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}
$GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
$gameExe = Join-Path $GameRoot 'Minecraft.Windows.exe'
if (Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $gameExe }) {
    throw 'Exit Minecraft first. This script never stops the game.'
}
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packPrefix = 'mods/LHolo/resource_packs/LHoloUI/'
$files = @('mods/LHolo/LHolo.dll', 'mods/LHolo/manifest.json', 'mods/LHolo/LICENSE',
    ($packPrefix + 'manifest.json'), ($packPrefix + 'hbui/lholo-menu.html'), ($packPrefix + 'hbui/lholo-menu.js'))
$legacy = @('data/gui/dist/hbui/lholo-menu.html', 'data/gui/dist/hbui/lholo-menu.js')
$packageRoot = Join-Path $projectRoot 'bin'
$reportPath = Join-Path $projectRoot 'build/oreui/package-validation.json'
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
# Validate every source before touching the game directory.
foreach ($relative in $files) {
    $packagePath = $relative.Substring('mods/'.Length)
    $source = Join-Path $packageRoot $packagePath
    $expected = $report.files.$packagePath
    if (!$expected -or !(Test-Path -LiteralPath $source -PathType Leaf) -or
        (Get-LHoloFileHash $source) -ne $expected) {
        throw "Missing or outdated package file: $packagePath. Run xmake first."
    }
}
foreach ($relative in $legacy) {
    $path = Join-Path $GameRoot $relative
    if ((Test-Path -LiteralPath $path) -and !(Get-Content -LiteralPath $path -Raw).Contains('lholo-menu-v7-settings')) {
        throw "Legacy file is not the expected LHolo v7 test asset: $path"
    }
}
foreach ($relative in $files) {
    $destination = Join-Path $GameRoot $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    $source = Join-Path $packageRoot ($relative.Substring('mods/'.Length))
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-LHoloFileHash $destination) -ne (Get-LHoloFileHash $source)) {
        throw "Deployment hash mismatch: $relative"
    }
}
foreach ($relative in $legacy) {
    $path = Join-Path $GameRoot $relative
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
Write-Host 'Verified complete LHolo deployment from bin/LHolo.'
Write-Host 'Start Minecraft yourself; use the menu hotkey or chat LHolo to test.'
