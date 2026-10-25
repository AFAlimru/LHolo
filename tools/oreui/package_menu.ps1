param(
    [Parameter(Mandatory = $true)][string]$Hbui
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
& python (Join-Path $PSScriptRoot 'build_menu.py') $Hbui
if ($LASTEXITCODE -ne 0) { throw 'OreUI asset build failed.' }
& node --check (Join-Path $projectRoot 'build\oreui\lholo-menu.js')
if ($LASTEXITCODE -ne 0) { throw 'OreUI generated script failed syntax validation.' }
$packageRoot = Join-Path $projectRoot 'build\oreui-test-package'
$modRoot = Join-Path $packageRoot 'mods\LHolo'
$uiRoot = Join-Path $packageRoot 'data\gui\dist\hbui'
foreach ($name in @('LHolo.dll', 'manifest.json', 'LICENSE')) {
    if (!(Test-Path -LiteralPath (Join-Path $projectRoot "bin\LHolo\$name") -PathType Leaf)) {
        throw "Build the DLL first: missing $name"
    }
}
New-Item -ItemType Directory -Force -Path $modRoot, $uiRoot | Out-Null
foreach ($name in @('LHolo.dll', 'manifest.json', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "bin\LHolo\$name") -Destination (Join-Path $modRoot $name) -Force
}
foreach ($name in @('lholo-menu.html', 'lholo-menu.js')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "build\oreui\$name") -Destination (Join-Path $uiRoot $name) -Force
}
Write-Host "Local test package: $packageRoot"
Write-Host 'Exit Minecraft before deploying. Chat LHolo opens OreUI; Alt+M opens the existing UI.'
