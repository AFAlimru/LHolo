param(
    [string]$GameRoot = 'D:\games\LeviLauncher\MC\versions\1.26.51.01'
)
$ErrorActionPreference = 'Stop'
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
$testRoot = Join-Path $projectRoot 'build/oreui-resource-pack-test'
$report = Get-Content -LiteralPath (Join-Path $testRoot 'validation.json') -Raw | ConvertFrom-Json
$archive = Join-Path $testRoot "LHolo-$($report.version)-resource-pack-local-test.zip"
if ((Get-FileHash -LiteralPath $archive).Hash -ne $report.archiveSha256) { throw 'Test archive hash mismatch.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
try {
    if ($zip.Entries.Count -ne $files.Count) { throw 'Unexpected ZIP inventory.' }
    $staging = Join-Path $testRoot ('deploy-' + [guid]::NewGuid().ToString('N'))
    foreach ($relative in $files) {
        $zipPath = $relative.Substring('mods/'.Length)
        $entry = $zip.GetEntry($zipPath)
        if (!$entry) { throw "Missing ZIP entry: $zipPath" }
        $destination = Join-Path $staging $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destination)
        if ((Get-FileHash -LiteralPath $destination).Hash -ne $report.files.$zipPath) { throw 'Staging hash mismatch.' }
    }
} finally { $zip.Dispose() }
foreach ($relative in $legacy) {
    $path = Join-Path $GameRoot $relative
    if ((Test-Path -LiteralPath $path) -and !(Get-Content -LiteralPath $path -Raw).Contains('lholo-menu-v7-settings')) {
        throw "Legacy file is not the expected LHolo v7 test asset: $path"
    }
}
foreach ($relative in $files) {
    $destination = Join-Path $GameRoot $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $staging $relative) -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath (Join-Path $staging $relative)).Hash) {
        throw "Deployment hash mismatch: $relative"
    }
}
foreach ($relative in $legacy) {
    $path = Join-Path $GameRoot $relative
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
Write-Host 'Verified resource-pack test deployment (direct overwrite; no backups).'
Write-Host 'Start Minecraft yourself and send LHolo in chat. Pack mounting / page loading remain unverified.'
