<#
.SYNOPSIS
  Installer for Bing Wallpaper Script.

.DESCRIPTION
  Downloads Set-BingWallpaper.ps1 from GitHub into AppData and registers
  a Windows Task Scheduler daily job to run it.
#>

# --- Default values (can be overridden by setting these before iwr|iex) ---
if (-not $TaskName)   { $TaskName   = "Set Bing Wallpaper Daily" }
if (-not $Time)       { $Time       = "08:00AM" }
if (-not $RepoRawUrl) { $RepoRawUrl = "https://raw.githubusercontent.com/zacuke/set-bingwallpaper/refs/heads/main/Set-BingWallpaper.ps1" }

# --- Destination path for the script (persistent under AppData) ---
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

# Remove existing task if any
if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
}

# Create a daily trigger
$trigger = New-ScheduledTaskTrigger -Daily -At ([DateTime]::Parse($Time))

# Define the action: PowerShell launching the downloaded script
$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-ExecutionPolicy Bypass -NoProfile -File `"$scriptPath`""

# Register the task under the current user
Register-ScheduledTask -Action $action -Trigger $trigger -TaskName $TaskName -Description "Automatically sets Bing wallpaper daily" -User $env:USERNAME -RunLevel Limited

Write-Host "✅ Installed!"
Write-Host "   Script stored at: $scriptPath"
Write-Host "   Task name: $TaskName (runs daily at $Time)"