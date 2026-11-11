param(
    [string]$Version = "1.1.0",
    [string]$ExecutablePath,
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $ProjectRoot "build\windows-msvc\Release\Quickdraw.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $ProjectRoot "out"
}

$ExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$InstallerScript = Join-Path $ProjectRoot "installer\quickdraw.iss"
$ExpectedInstaller = Join-Path $OutputDirectory "Quickdraw-Setup-$Version.exe"

if (-not (Test-Path $ExecutablePath -PathType Leaf)) {
    throw "Quickdraw executable was not found at $ExecutablePath."
}

$CompilerCandidates = @(
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 7\ISCC.exe"),
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 7\ISCC.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 7\ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe")
)
$Compiler = $CompilerCandidates |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path $_ -PathType Leaf) } |
    Select-Object -First 1

if ($null -eq $Compiler) {
    throw "Inno Setup Compiler (ISCC.exe) was not found. Install Inno Setup 6 or 7."
}

New-Item $OutputDirectory -ItemType Directory -Force | Out-Null
if (Test-Path $ExpectedInstaller) {
    Remove-Item $ExpectedInstaller -Force
}

& $Compiler `
    "/DAppVersion=$Version" `
    "/DSourceExecutable=$ExecutablePath" `
    "/DOutputDirectory=$OutputDirectory" `
    $InstallerScript

if ($LASTEXITCODE -ne 0) {
    throw "Installer compilation failed with exit code $LASTEXITCODE."
}
if (-not (Test-Path $ExpectedInstaller -PathType Leaf)) {
    throw "Installer compilation completed without producing $ExpectedInstaller."
}

Write-Host "Created $ExpectedInstaller"
