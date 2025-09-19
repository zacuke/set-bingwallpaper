<#
.SYNOPSIS
  Uninstaller for Bing Wallpaper Script.

.DESCRIPTION
  Removes the scheduled task and deletes the AppData folder where the script was installed.
#>

param (
    [string]$TaskName = "Set Bing Wallpaper Daily"
)

$installDir = Join-Path $env:LOCALAPPDATA "BingWallpaper"

Write-Host "Uninstalling Bing Wallpaper script..."

# Remove Scheduled Task
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Write-Host "  Removing Scheduled Task: $TaskName"
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
} else {
    Write-Host "  No scheduled task found named '$TaskName'."
}

# Remove install directory
if (Test-Path $installDir) {
    Write-Host "  Removing install directory: $installDir"
    Remove-Item -Path $installDir -Recurse -Force
} else {
    Write-Host "  No install directory found at $installDir"
}

Write-Host "Uninstall complete."