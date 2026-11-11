param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath
)

$ErrorActionPreference = "Stop"

$SourceInstallerPath = [System.IO.Path]::GetFullPath($InstallerPath)
if (-not (Test-Path $SourceInstallerPath -PathType Leaf)) {
    throw "Installer was not found at $SourceInstallerPath."
}

$TestId = [Guid]::NewGuid().ToString("N")
$InstallDirectory = Join-Path $env:TEMP "QuickdrawInstallerTest-$TestId"
$LogDirectory = Join-Path $env:TEMP "QuickdrawInstallerLogs-$TestId"
$DataDirectory = Join-Path $env:LOCALAPPDATA "Quickdraw"
$DataDirectoryExisted = Test-Path $DataDirectory -PathType Container
$SentinelPath = Join-Path $DataDirectory "installer-test-$TestId.sentinel"
$StartMenuShortcut = Join-Path ([Environment]::GetFolderPath("Programs")) "Quickdraw.lnk"
$DesktopShortcut = Join-Path ([Environment]::GetFolderPath("Desktop")) "Quickdraw.lnk"
$RunKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$RunValueName = "Quickdraw"
$PortableCommand = '"C:\Portable\Quickdraw.exe"'

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-StartupCommand {
    try {
        return Get-ItemPropertyValue -Path $RunKey -Name $RunValueName -ErrorAction Stop
    } catch [System.Management.Automation.ItemNotFoundException] {
        return $null
    } catch [System.Management.Automation.PSArgumentException] {
        return $null
    }
}

function Set-StartupCommand {
    param([AllowNull()]$Command)

    if ($null -eq $Command) {
        Remove-ItemProperty -Path $RunKey -Name $RunValueName -ErrorAction SilentlyContinue
        return
    }

    New-Item $RunKey -Force | Out-Null
    Set-ItemProperty -Path $RunKey -Name $RunValueName -Value $Command -Type String
}

function Invoke-Installer {
    param([string[]]$AdditionalArguments = @())

    $Arguments = @(
        "/SP-",
        "/VERYSILENT",
        "/SUPPRESSMSGBOXES",
        "/NORESTART",
        "/DIR=`"$InstallDirectory`"",
        "/LOG=`"$(Join-Path $LogDirectory 'setup.log')`""
    ) + $AdditionalArguments

    $Process = Start-Process `
        -FilePath $InstallerPath `
        -ArgumentList $Arguments `
        -PassThru `
        -Wait
    if ($Process.ExitCode -ne 0) {
        throw "Installer exited with code $($Process.ExitCode)."
    }
}

function Find-UninstallRegistration {
    return Get-ChildItem "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall" `
        -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ItemProperty $_.PSPath } |
        Where-Object {
            $_.DisplayName -eq "Quickdraw" -and
            $_.InstallLocation.TrimEnd("\") -eq $InstallDirectory.TrimEnd("\")
        } |
        Select-Object -First 1
}

function Assert-Installed {
    $ExpectedStartup = '"' + (Join-Path $InstallDirectory "Quickdraw.exe") + '"'
    $ActualStartup = Get-StartupCommand
    Assert-Condition (Test-Path (Join-Path $InstallDirectory "Quickdraw.exe") -PathType Leaf) `
        "Installed Quickdraw.exe is missing."
    Assert-Condition (Test-Path (Join-Path $InstallDirectory "LICENSE") -PathType Leaf) `
        "Installed license is missing."
    Assert-Condition (Test-Path $StartMenuShortcut -PathType Leaf) `
        "Start menu shortcut is missing."
    Assert-Condition (Test-Path $DesktopShortcut -PathType Leaf) `
        "Desktop shortcut is missing."
    Assert-Condition ($ActualStartup -eq $ExpectedStartup) `
        "Startup entry mismatch. Expected '$ExpectedStartup'; actual '$ActualStartup'."
    Assert-Condition ($null -ne (Find-UninstallRegistration)) `
        "Apps & features registration is missing."
}

function Invoke-Uninstaller {
    $UninstallerPath = Join-Path $InstallDirectory "unins000.exe"
    if (-not (Test-Path $UninstallerPath -PathType Leaf)) {
        throw "Uninstaller is missing at $UninstallerPath."
    }

    $Process = Start-Process `
        -FilePath $UninstallerPath `
        -ArgumentList @(
            "/VERYSILENT",
            "/SUPPRESSMSGBOXES",
            "/NORESTART",
            "/LOG=`"$(Join-Path $LogDirectory 'uninstall.log')`""
        ) `
        -PassThru `
        -Wait
    if ($Process.ExitCode -ne 0) {
        throw "Uninstaller exited with code $($Process.ExitCode)."
    }
}

$OriginalStartupCommand = Get-StartupCommand
$TestSucceeded = $false

if (Test-Path $StartMenuShortcut -PathType Leaf) {
    throw "Refusing to replace an existing Quickdraw Start menu shortcut."
}
if (Test-Path $DesktopShortcut -PathType Leaf) {
    throw "Refusing to replace an existing Quickdraw desktop shortcut."
}
if ($null -ne (Find-UninstallRegistration)) {
    throw "Refusing to replace an existing Quickdraw installation."
}

New-Item $LogDirectory -ItemType Directory -Force | Out-Null
$InstallerPath = Join-Path $LogDirectory (Split-Path $SourceInstallerPath -Leaf)
Copy-Item $SourceInstallerPath $InstallerPath
Unblock-File $InstallerPath
New-Item $DataDirectory -ItemType Directory -Force | Out-Null
Set-Content $SentinelPath "preserve" -Encoding ascii

try {
    Invoke-Installer @('/TASKS="startup,desktopicon"')
    Assert-Installed

    # An upgrade without task switches follows the current registry state.
    Invoke-Installer
    Assert-Installed

    # An explicit unattended task override disables startup.
    Invoke-Installer @('/MERGETASKS="!startup"')
    Assert-Condition ($null -eq (Get-StartupCommand)) `
        "Explicit startup disable did not remove the startup entry."

    # Re-enable startup, then prove a normal uninstall removes installer ownership.
    Invoke-Installer @('/MERGETASKS="startup"')
    Assert-Installed
    Invoke-Uninstaller
    Assert-Condition (-not (Test-Path $InstallDirectory)) `
        "Installation directory remains after uninstall."
    Assert-Condition (-not (Test-Path $StartMenuShortcut)) `
        "Start menu shortcut remains after uninstall."
    Assert-Condition (-not (Test-Path $DesktopShortcut)) `
        "Desktop shortcut remains after uninstall."
    Assert-Condition ($null -eq (Get-StartupCommand)) `
        "Installer-owned startup entry remains after uninstall."
    Assert-Condition (Test-Path $SentinelPath -PathType Leaf) `
        "Uninstall removed Quickdraw user data."

    # A startup entry redirected to a portable copy does not belong to uninstall.
    Invoke-Installer @('/TASKS="startup,desktopicon"')
    Set-StartupCommand $PortableCommand
    Invoke-Uninstaller
    Assert-Condition ((Get-StartupCommand) -eq $PortableCommand) `
        "Uninstall removed a startup entry owned by a portable copy."
    Assert-Condition (Test-Path $SentinelPath -PathType Leaf) `
        "Second uninstall removed Quickdraw user data."

    $TestSucceeded = $true
    Write-Host "Installer lifecycle tests passed."
} finally {
    $RemainingUninstaller = Join-Path $InstallDirectory "unins000.exe"
    if (Test-Path $RemainingUninstaller -PathType Leaf) {
        Start-Process `
            -FilePath $RemainingUninstaller `
            -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") `
            -Wait | Out-Null
    }

    Set-StartupCommand $OriginalStartupCommand
    Remove-Item $SentinelPath -Force -ErrorAction SilentlyContinue
    if (-not $DataDirectoryExisted -and
        (Test-Path $DataDirectory -PathType Container) -and
        $null -eq (Get-ChildItem $DataDirectory -Force | Select-Object -First 1)) {
        Remove-Item $DataDirectory -Force
    }
    Remove-Item $InstallDirectory -Recurse -Force -ErrorAction SilentlyContinue
    if ($TestSucceeded) {
        Remove-Item $LogDirectory -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "Installer test logs retained at $LogDirectory"
    }
}
