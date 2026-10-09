param(
    [Parameter(Mandatory = $true)][string]$ProfileName,
    [string]$InterfaceName = 'Wi-Fi'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$tempFolder = Join-Path ([IO.Path]::GetTempPath()) ('rad-wifi-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempFolder | Out-Null
try {
    # Keep the exported key local; never print the XML or keyMaterial.
    $output = & netsh wlan export profile "name=$ProfileName" "interface=$InterfaceName" "folder=$tempFolder" key=clear 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Exporteren van het Windows-wifiprofiel is mislukt.' }
    $files = @(Get-ChildItem -LiteralPath $tempFolder -Filter '*.xml')
    if ($files.Count -ne 1) { throw 'Geen uniek wifiprofiel gevonden.' }
    [xml]$profile = Get-Content -LiteralPath $files[0].FullName -Raw
    $ssid = [string]$profile.WLANProfile.SSIDConfig.SSID.name
    $keyNode = $profile.WLANProfile.MSM.security.sharedKey
    $password = ''
    if ($keyNode) {
        if ([string]$keyNode.protected -ne 'false') { throw 'Windows geeft het wachtwoord niet vrij. Configureer include/wifi_secrets.h handmatig.' }
        $password = [string]$keyNode.keyMaterial
    } elseif ([string]$profile.WLANProfile.MSM.security.authEncryption.authentication -ne 'open') {
        throw 'Dit script ondersteunt open wifi en persoonlijke wifi met een opgeslagen sleutel.'
    }
    function ConvertTo-CppBytes([string]$Value) {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Value)
        return '"' + (($bytes | ForEach-Object { '\x' + $_.ToString('X2') }) -join '') + '"'
    }
    $header = "#pragma once`n// Private local configuration; do not commit.`n#define RAD_WIFI_SSID $(ConvertTo-CppBytes $ssid)`n#define RAD_WIFI_PASSWORD $(ConvertTo-CppBytes $password)`n"
    [IO.File]::WriteAllText((Join-Path $projectRoot 'include\wifi_secrets.h'), $header, [Text.Encoding]::ASCII)
    Write-Host 'Wifi ingesteld. Het wachtwoord is niet getoond.'
} finally {
    # Delete only files in the exact temporary directory created above.
    if (Test-Path -LiteralPath $tempFolder) {
        Get-ChildItem -LiteralPath $tempFolder -File | Remove-Item -Force
        Remove-Item -LiteralPath $tempFolder -Force
    }
    $password = $null
    $header = $null
    $profile = $null
}
