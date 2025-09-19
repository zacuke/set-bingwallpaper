<#
.SYNOPSIS
  Installer for Bing Wallpaper Script.

.DESCRIPTION
  Downloads Set-BingWallpaper.ps1 from GitHub into AppData and registers
  a Windows Task Scheduler daily job to run it.
#>

param (
    [string]$TaskName = "Set Bing Wallpaper Daily"
    [string]$Time = "08:00AM"
    [string]$RepoRawUrl = "https://raw.githubusercontent.com/zacuke/set-bingwallpaper/main/Set-BingWallpaper.ps1"
)

# Destination directory for the script (persistent location under AppData)
$installDir = Join-Path $env:LOCALAPPDATA "BingWallpaper"
$scriptPath = Join-Path $installDir "Set-BingWallpaper.ps1"

# Ensure folder exists
if (!(Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir | Out-Null
}

Write-Host "⬇️ Downloading Set-BingWallpaper.ps1 from $RepoRawUrl"
Invoke-WebRequest -Uri $RepoRawUrl -OutFile $scriptPath -UseBasicParsing

if (!(Test-Path $scriptPath)) {
    Write-Error "❌ Failed to download the wallpaper script."
    exit 1
}

# If task already exists, remove it
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
}

# Create trigger (daily at user-specified time)
$trigger = New-ScheduledTaskTrigger -Daily -At ([DateTime]::Parse($Time))

# Define action: PowerShell launching the downloaded script
$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-ExecutionPolicy Bypass -NoProfile -File `"$scriptPath`""

# Register the task under the current account
Register-ScheduledTask -Action $action -Trigger $trigger -TaskName $TaskName -Description "Automatically sets Bing wallpaper daily" -User $env:USERNAME -RunLevel Limited

Write-Host "✅ Installed!"
Write-Host "   Script stored at: $scriptPath"
Write-Host "   Task name: $TaskName (runs daily at $Time)"