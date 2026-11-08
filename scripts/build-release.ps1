param(
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot "build\windows-msvc"
$OutputDirectory = Join-Path $ProjectRoot "out"
$StageDirectory = Join-Path $OutputDirectory "Quickdraw-$Version-win-x64"
$ArchivePath = "$StageDirectory.zip"

cmake --preset windows-msvc
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE."
}

cmake --build --preset windows-release
if ($LASTEXITCODE -ne 0) {
    throw "Release build failed with exit code $LASTEXITCODE."
}

ctest --test-dir $BuildDirectory -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE."
}

if (Test-Path $StageDirectory) {
    Remove-Item $StageDirectory -Recurse -Force
}
if (Test-Path $ArchivePath) {
    Remove-Item $ArchivePath -Force
}

New-Item $OutputDirectory -ItemType Directory -Force | Out-Null
New-Item $StageDirectory -ItemType Directory | Out-Null
Copy-Item (Join-Path $BuildDirectory "Release\Quickdraw.exe") $StageDirectory
Copy-Item (Join-Path $ProjectRoot "README.md") $StageDirectory
Copy-Item (Join-Path $ProjectRoot "LICENSE") $StageDirectory
Copy-Item (Join-Path $ProjectRoot "docs\windows-test-matrix.md") $StageDirectory

Compress-Archive -Path "$StageDirectory\*" -DestinationPath $ArchivePath
$Hash = Get-FileHash $ArchivePath -Algorithm SHA256
$Hash.Hash.ToLowerInvariant() + "  " + (Split-Path $ArchivePath -Leaf) |
    Set-Content "$ArchivePath.sha256" -Encoding ascii

Write-Host "Created $ArchivePath"
Write-Host "SHA-256: $($Hash.Hash)"
