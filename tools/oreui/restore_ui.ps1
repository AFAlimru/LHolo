param(
    [Parameter(Mandatory = $true)][string]$Checkpoint,
    [switch]$RestoreSource
)
$ErrorActionPreference = 'Stop'
$checkpointRoot = (Resolve-Path -LiteralPath $Checkpoint).Path
$manifest = Get-Content -LiteralPath (Join-Path $checkpointRoot 'manifest.json') -Raw | ConvertFrom-Json
$gameRoot = [System.IO.Path]::GetFullPath($manifest.gameRoot)
$gameExe = Join-Path $gameRoot 'Minecraft.Windows.exe'
if (Get-Process -Name 'Minecraft.Windows' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $gameExe }) {
    throw 'Exit Minecraft before restoring UI files. This script does not stop the game.'
}
# Validate every source before writing any installed file.
foreach ($entry in $manifest.files) {
    $source = Join-Path (Join-Path $checkpointRoot 'deployed') $entry.path
    $destination = [System.IO.Path]::GetFullPath((Join-Path $gameRoot $entry.path))
    if (!$destination.StartsWith($gameRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Invalid checkpoint destination: $destination"
    }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Checkpoint hash mismatch: $source"
    }
}
foreach ($entry in $manifest.files) {
    $source = Join-Path (Join-Path $checkpointRoot 'deployed') $entry.path
    $destination = Join-Path $gameRoot $entry.path
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Restore verification failed: $destination"
    }
}
if ($RestoreSource) {
    # Restore only this trial's edited files, not unrelated workspace changes.
    $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $paths = @(
        'frontend/src/platform/native.tsx', 'tools/oreui/runtime_adapter.js',
        'frontend/src/components/MenuNavigation.tsx', 'frontend/src/LHoloMenu.tsx',
        'frontend/src/pages/ProjectionPage.tsx', 'frontend/src/styles/menu.css',
        'frontend/tests/menu.cjs', 'frontend/README.md', 'src/oreui/README.md',
        'tools/oreui/README.md', 'docs/OREUI_MIGRATION.md', 'DEVELOPMENT.md',
        'README.md', 'docs/README_EN.md', 'AGENTS.md'
    )
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $checkpointRoot $manifest.sourceArchive))
    try {
        foreach ($relative in $paths) {
            $entry = $archive.GetEntry($relative)
            if (!$entry) { throw "Source checkpoint missing: $relative" }
        }
        foreach ($relative in $paths) {
            [IO.Compression.ZipFileExtensions]::ExtractToFile($archive.GetEntry($relative), (Join-Path $projectRoot $relative), $true)
        }
    } finally { $archive.Dispose() }
}
Write-Host 'Previous UI restored and hashes verified. Source restoration:' $RestoreSource
