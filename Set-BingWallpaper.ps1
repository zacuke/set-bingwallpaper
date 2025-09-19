# Define save path (you can change this)
$savePath = "$env:USERPROFILE\Pictures\BingWallpapers"
if (!(Test-Path $savePath)) {
    New-Item -ItemType Directory -Path $savePath | Out-Null
}

# Get Bing JSON feed for images
$bingApi = "https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=1&mkt=en-US"
$response = Invoke-RestMethod -Uri $bingApi

# Extract image URL
$imgUrl = "https://www.bing.com" + $response.images[0].url
$fileName = ($response.images[0].startdate) + ".jpg"
$filePath = Join-Path $savePath $fileName

# Download wallpaper if not already saved
if (!(Test-Path $filePath)) {
    Invoke-WebRequest -Uri $imgUrl -OutFile $filePath
}

# Set as desktop background
Add-Type @"
using System.Runtime.InteropServices;
public class Wallpaper {
  [DllImport("user32.dll",SetLastError=true)]
  public static extern bool SystemParametersInfo(int uAction, int uParam, string lpvParam, int fuWinIni);
}
"@

$SPI_SETDESKWALLPAPER = 0x14
$SPIF_UPDATEINIFILE = 0x01
$SPIF_SENDWININICHANGE = 0x02
[Wallpaper]::SystemParametersInfo($SPI_SETDESKWALLPAPER, 0, $filePath, $SPIF_UPDATEINIFILE -bor $SPIF_SENDWININICHANGE)