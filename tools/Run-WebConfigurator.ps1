param([int]$Port = 8765, [switch]$Lan, [string]$DataDir = '.minidash')
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repositoryRoot
$bindAddress = if ($Lan) { '0.0.0.0' } else { '127.0.0.1' }
Write-Host "MiniDash simulation: http://127.0.0.1:$Port"
if ($Lan) {
    Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
        $_.IPAddress -ne '127.0.0.1' -and $_.IPAddress -notlike '169.254*'
    } | ForEach-Object { Write-Host "Phone on the same network: http://$($_.IPAddress):$Port" }
}
python -m minidash_live.configurator --host $bindAddress --port $Port --data-dir $DataDir
exit $LASTEXITCODE
