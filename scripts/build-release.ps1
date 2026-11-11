param(
    [string]$Version = "1.1.0"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot "build\windows-msvc"
$OutputDirectory = Join-Path $ProjectRoot "out"
$StageDirectory = Join-Path $OutputDirectory "Quickdraw-$Version-win-x64"
$ArchivePath = "$StageDirectory.zip"
$PortablePath = Join-Path $OutputDirectory "Quickdraw.exe"
$InstallerPath = Join-Path $OutputDirectory "Quickdraw-Setup-$Version.exe"

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

& (Join-Path $PSScriptRoot "build-installer.ps1") `
    -Version $Version `
    -ExecutablePath (Join-Path $BuildDirectory "Release\Quickdraw.exe") `
    -OutputDirectory $OutputDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Installer build failed with exit code $LASTEXITCODE."
}

& (Join-Path $PSScriptRoot "test-installer.ps1") -InstallerPath $InstallerPath
if ($LASTEXITCODE -ne 0) {
    throw "Installer tests failed with exit code $LASTEXITCODE."
}

if (Test-Path $StageDirectory) {
    Remove-Item $StageDirectory -Recurse -Force
}
if (Test-Path $ArchivePath) {
    Remove-Item $ArchivePath -Force
}

New-Item $OutputDirectory -ItemType Directory -Force | Out-Null
New-Item $StageDirectory -ItemType Directory | Out-Null
Copy-Item (Join-Path $BuildDirectory "Release\Quickdraw.exe") $PortablePath
Copy-Item $PortablePath $StageDirectory
Copy-Item (Join-Path $ProjectRoot "README.md") $StageDirectory
Copy-Item (Join-Path $ProjectRoot "LICENSE") $StageDirectory
Copy-Item (Join-Path $ProjectRoot "docs\windows-test-matrix.md") $StageDirectory

Compress-Archive -Path "$StageDirectory\*" -DestinationPath $ArchivePath
$Artifacts = @($PortablePath, $InstallerPath, $ArchivePath)
foreach ($Artifact in $Artifacts) {
    $Hash = Get-FileHash $Artifact -Algorithm SHA256
    $Hash.Hash.ToLowerInvariant() + "  " + (Split-Path $Artifact -Leaf) |
        Set-Content "$Artifact.sha256" -Encoding ascii
    Write-Host "Created $Artifact"
    Write-Host "SHA-256: $($Hash.Hash)"
}
