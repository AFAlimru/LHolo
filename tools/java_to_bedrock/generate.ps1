param(
    [Parameter(Mandatory = $true)]
    [string] $ChunkerRoot
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$chunker = (Resolve-Path -LiteralPath $ChunkerRoot).Path
$gradle = Join-Path $chunker 'gradlew.bat'

Push-Location $chunker
try {
    & $gradle :cli:shadowJar -x test
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

$jar = Get-ChildItem -LiteralPath (Join-Path $chunker 'cli\build\libs') -Filter 'chunker-cli-*.jar' |
    Where-Object { $_.Name -notlike '*unshaded*' } |
    Select-Object -First 1
if (-not $jar) { throw 'Chunker shaded CLI jar was not produced.' }

$classes = Join-Path $projectRoot 'build\java-to-bedrock-generator'
New-Item -ItemType Directory -Force -Path $classes | Out-Null
$source = Join-Path $PSScriptRoot 'GenerateMappings.java'
$output = Join-Path $projectRoot 'src\structure\java_to_bedrock\GeneratedChunkerMappings.inc'

& javac -encoding UTF-8 -cp $jar.FullName -d $classes $source
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& java -Xmx4G -cp "$classes;$($jar.FullName)" GenerateMappings $chunker $output
exit $LASTEXITCODE
