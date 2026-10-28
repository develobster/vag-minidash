param(
    [Parameter(Mandatory=$true)][string]$Port,
    [string]$Python='python',
    [string]$FirmwareDirectory,
    [string]$BackupDirectory,
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
if([string]::IsNullOrWhiteSpace($FirmwareDirectory)) { $FirmwareDirectory=$PSScriptRoot }
if([string]::IsNullOrWhiteSpace($BackupDirectory)) { $BackupDirectory=Join-Path $FirmwareDirectory '..\..\nerdminer-backup' }
$backup=Get-Content -LiteralPath (Join-Path $BackupDirectory 'backup-manifest.json') -Raw | ConvertFrom-Json
$backupPath=Join-Path $BackupDirectory $backup.filename
if($backup.status -ne 'VERIFIED' -or (Get-Item -LiteralPath $backupPath).Length -ne 4194304 -or (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $backup.sha256) {
    throw 'Original NerdMiner backup integrity check failed. Nothing was written.'
}
$manifest=Get-Content -LiteralPath (Join-Path $FirmwareDirectory 'firmware-manifest.json') -Raw | ConvertFrom-Json
$image=Join-Path $FirmwareDirectory $manifest.app_filename
if((Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.app_sha256) {throw 'Application checksum mismatch.'}
$identity=& $Python -m esptool --chip esp32 --port $Port read-mac 2>&1
if($LASTEXITCODE -ne 0 -or ($identity -join "`n") -notmatch [regex]::Escape($backup.mac)) {throw 'Connected chip does not match the verified backup.'}
if($ValidateOnly) {Write-Host 'Backup, application and board identity verified. No flash write performed.';return}
& $Python -m esptool --chip esp32 --port $Port --baud 115200 --after no-reset write-flash 0x10000 $image
if($LASTEXITCODE -ne 0) {throw 'Application write failed. Original backup is available separately.'}
& $Python -m esptool --chip esp32 --port $Port --baud 115200 --before no-reset --after hard-reset verify-flash 0x10000 $image
if($LASTEXITCODE -ne 0) {throw 'Application verification failed.'}
Write-Host 'MiniDash CYD application verified. NVS was preserved. Device restarted.'
