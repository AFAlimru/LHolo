param(
    [string]$GameRoot = 'D:\games\LeviLauncher\MC\versions\1.26.51.01'
)
$ErrorActionPreference = 'Stop'
$GameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
$gameExe = Join-Path $GameRoot 'Minecraft.Windows.exe'
if (Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $gameExe }) {
    throw 'Exit Minecraft before updating the DLL and UI together. This script does not stop the game.'
}
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$packageRoot = Join-Path $projectRoot 'build\oreui-test-package'
$files = @('mods\LHolo\LHolo.dll', 'mods\LHolo\manifest.json', 'mods\LHolo\LICENSE',
    'data\gui\dist\hbui\lholo-menu.html', 'data\gui\dist\hbui\lholo-menu.js')
foreach ($relative in $files) {
    if (!(Test-Path -LiteralPath (Join-Path $packageRoot $relative) -PathType Leaf)) {
        throw "Missing package file: $relative"
    }
}
foreach ($name in @('lholo-menu.html', 'lholo-menu.js')) {
    $source = Join-Path $packageRoot "data\gui\dist\hbui\$name"
    if (!(Get-Content -LiteralPath $source -Raw).Contains('lholo-menu-v7-settings')) {
        throw "Unsupported UI protocol: $source"
    }
}
foreach ($relative in $files) {
    $source = Join-Path $packageRoot $relative
    $destination = Join-Path $GameRoot $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash) {
        throw "Deployment hash mismatch: $destination"
    }
    Write-Host "Verified $destination"
}
Write-Host 'Deployment complete. Start Minecraft yourself; chat LHolo, then click Browse.'
