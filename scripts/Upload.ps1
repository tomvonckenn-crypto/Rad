param([string]$Address)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.venv\Scripts\python.exe'
if (!$Address) {
    $addressFile = Join-Path $projectRoot '.local\wemos-address.txt'
    if (Test-Path -LiteralPath $addressFile) { $Address = (Get-Content -LiteralPath $addressFile -Raw).Trim() }
    else { $Address = 'rad-wemos.local' }
}
Push-Location $projectRoot
try {
    # Build on the current wifi, then use the base Python already allowed by Firewall.
    & $python -m platformio run -e rad
    if ($LASTEXITCODE -ne 0) { throw 'Build mislukt; geen upload uitgevoerd.' }
    $otaPython = & $python -c 'import os,sys; print(os.path.realpath(sys._base_executable))'
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $otaPython)) { throw 'Upload-Python niet gevonden.' }
    $coreDirectory = & $python -c "from platformio.project.config import ProjectConfig; print(ProjectConfig.get_instance().get('platformio', 'core_dir'))"
    if ($LASTEXITCODE -ne 0) { throw 'PlatformIO-map niet gevonden.' }
    $uploader = Join-Path $coreDirectory 'packages\framework-arduinoespressif8266\tools\espota.py'
    $firmware = Join-Path $projectRoot '.pio\build\rad\firmware.bin'
    if (!(Test-Path -LiteralPath $uploader) -or !(Test-Path -LiteralPath $firmware)) { throw 'Uploader of firmware ontbreekt.' }
    Write-Host "Upload naar $Address via gewone wifi; Upload-Python: $otaPython"
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $uploadOutput = & $otaPython $uploader "--ip=$Address" --host_port=8267 --file=$firmware 2>&1
        $uploadExitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedPreference }
    $logFolder = Join-Path $projectRoot '.local'
    New-Item -ItemType Directory -Path $logFolder -Force | Out-Null
    $uploadLines = @($uploadOutput | ForEach-Object { $_.ToString() })
    $uploadLines | Set-Content -LiteralPath (Join-Path $logFolder 'ota-normal.log') -Encoding UTF8
    $uploadLines | ForEach-Object { Write-Host $_ }
    if ($uploadExitCode -ne 0) { throw 'OTA-upload mislukt. Details: .local\ota-normal.log. Controleer het adres en de terugverbinding op TCP 8267.' }
    Write-Host 'OTA-upload via gewone wifi afgerond.'
} finally { Pop-Location }
