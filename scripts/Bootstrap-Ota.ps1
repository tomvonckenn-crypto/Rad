param(
    [Parameter(Mandatory = $true)][string]$HomeProfile,
    [string]$InterfaceName = 'Wi-Fi',
    [string]$RadProfile = 'Rad-Wemos'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.venv\Scripts\python.exe'
if (!(Test-Path -LiteralPath $python)) { throw 'Voer eerst de installatie uit die in README.md staat.' }
if (!(Test-Path -LiteralPath (Join-Path $projectRoot 'include\wifi_secrets.h'))) { throw 'Voer eerst Configure-Wifi.ps1 uit.' }
$logFolder = Join-Path $projectRoot '.local'
New-Item -ItemType Directory -Path $logFolder -Force | Out-Null
Push-Location $projectRoot
try {
    # Download dependencies and compile while internet is still available.
    & $python -m platformio run -e rad_bootstrap
    if ($LASTEXITCODE -ne 0) { throw 'Build mislukt; wifi blijft op het gewone netwerk.' }
    # Resolve everything while online. The uploader itself uses only Python's
    # standard library, so use the base Python already allowed by Windows Firewall.
    # Calling PlatformIO again after switching wifi could download optional tools.
    $otaPython = & $python -c 'import os,sys; print(os.path.realpath(sys._base_executable))'
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $otaPython)) { throw 'Upload-Python niet gevonden; wifi blijft ongewijzigd.' }
    $coreDirectory = & $python -c "from platformio.project.config import ProjectConfig; print(ProjectConfig.get_instance().get('platformio', 'core_dir'))"
    if ($LASTEXITCODE -ne 0) { throw 'PlatformIO-map niet gevonden; wifi blijft ongewijzigd.' }
    $uploader = Join-Path $coreDirectory 'packages\framework-arduinoespressif8266\tools\espota.py'
    $firmware = Join-Path $projectRoot '.pio\build\rad_bootstrap\firmware.bin'
    if (!(Test-Path -LiteralPath $uploader) -or !(Test-Path -LiteralPath $firmware)) { throw 'Uploader of firmware ontbreekt; wifi blijft ongewijzigd.' }
    Write-Host "Upload-Python: $otaPython"
    Write-Host 'Eenmalige overgang: verbinden met Rad-Wemos, uploaden, daarna wifi herstellen.'
    try {
        & netsh wlan connect "name=$RadProfile" "interface=$InterfaceName"
        if ($LASTEXITCODE -ne 0) { throw 'Verbinden met Rad-Wemos mislukt. Controleer het opgeslagen Windows-profiel.' }
        $deadline = [DateTime]::UtcNow.AddSeconds(35)
        $ready = $false
        $ping = New-Object Net.NetworkInformation.Ping
        do {
            Start-Sleep -Seconds 1
            $wifiState = & netsh wlan show interfaces
            $onRad = (($wifiState -join "`n") -match ('(?m)^\s*SSID\s*:\s*' + [regex]::Escape($RadProfile) + '\s*$'))
            if ($onRad) {
                try { $ready = $ping.Send('192.168.4.1', 800).Status -eq 'Success' } catch { $ready = $false }
            }
        } until ($ready -or [DateTime]::UtcNow -ge $deadline)
        $ping.Dispose()
        if (!$ready) { throw 'Wemos niet bereikbaar op 192.168.4.1; upload niet uitgevoerd.' }
        # Run the already installed uploader directly; no downloads while offline.
        # Windows PowerShell treats native stderr as errors when redirected. Keep
        # log messages from interrupting the uploader, then check its exit code.
        $savedPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $uploadOutput = & $otaPython $uploader --ip=192.168.4.1 --host_port=8267 --file=$firmware 2>&1
            $uploadExitCode = $LASTEXITCODE
        } finally { $ErrorActionPreference = $savedPreference }
        $uploadLines = @($uploadOutput | ForEach-Object { $_.ToString() })
        $uploadLines | Set-Content -LiteralPath (Join-Path $logFolder 'ota.log') -Encoding UTF8
        $uploadLines | ForEach-Object { Write-Host $_ }
        if ($uploadExitCode -ne 0) {
            if (($uploadLines -join "`n") -match 'No response from device') {
                throw 'Wemos antwoordt, maar de TCP-terugverbinding op poort 8267 mislukt. Controleer de firewallregel voor de hierboven genoemde Upload-Python. Details: .local\ota.log.'
            }
            throw 'OTA-upload mislukt. De echte uploader-melding staat hierboven en in .local\ota.log.'
        }
        Write-Host 'OTA-upload afgerond.'
    } finally {
        & netsh wlan connect "name=$HomeProfile" "interface=$InterfaceName"
        if ($LASTEXITCODE -ne 0) { Write-Warning "Wifi herstellen is mislukt. Verbind met $HomeProfile." }
        else {
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do {
                Start-Sleep -Seconds 1
                $wifiState = (& netsh wlan show interfaces) -join "`n"
                $restored = $wifiState -match ('(?m)^\s*Profile\s*:\s*' + [regex]::Escape($HomeProfile) + '\s*$')
            } until ($restored -or [DateTime]::UtcNow -ge $deadline)
            if ($restored) { Write-Host 'Gewone wifi hersteld.' }
            else { Write-Warning 'Terugverbinden duurt langer; controleer de wifi van Windows.' }
        }
    }
} finally { Pop-Location }
