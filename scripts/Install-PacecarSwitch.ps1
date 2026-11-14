param([switch]$Remove)
$ErrorActionPreference = 'Stop'
$destination = Join-Path $env:LOCALAPPDATA 'SwitchWOA\Pacecar'
$executable = Join-Path $destination 'Pacecar.Switch.exe'
$shortcutPath = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Switch Pacecar.lnk'

# Only this per-user application is installed. No drivers or power modes change.
if (Get-Process -Name 'Pacecar.Switch' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable }) {
    throw 'Exit Switch Pacecar from its tray menu before updating or removing it.'
}
if ($Remove) {
    $run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    $entry = Get-ItemProperty -LiteralPath $run -Name 'Pacecar' -ErrorAction SilentlyContinue
    if ($entry -and $entry.Pacecar -like ('*' + $executable + '*')) {
        Remove-ItemProperty -LiteralPath $run -Name 'Pacecar'
    }
    if (Test-Path -LiteralPath $shortcutPath) { Remove-Item -LiteralPath $shortcutPath }
    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse }
    Write-Output 'Removed Switch Pacecar. Settings and all drivers were retained.'
    exit 0
}
foreach ($file in @('Pacecar.Switch.exe', 'Pacecar.Sensors.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $file))) { throw "Missing $file next to installer." }
}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach ($file in @('Pacecar.Switch.exe', 'Pacecar.Sensors.exe', 'Install-PacecarSwitch.ps1')) {
    $source = Join-Path $PSScriptRoot $file
    if ([IO.Path]::GetFullPath($source) -ne [IO.Path]::GetFullPath((Join-Path $destination $file))) {
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
}
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'arm64')) {
    if ([IO.Path]::GetFullPath($PSScriptRoot) -ne [IO.Path]::GetFullPath($destination)) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'arm64') -Destination $destination -Recurse -Force
    }
}
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $executable
$shortcut.WorkingDirectory = $destination
$shortcut.Description = 'Switch ARM64 SoC telemetry and optional Present FPS'
$shortcut.Save()
Write-Output "Installed $executable and desktop shortcut. Launch it from the desktop."
