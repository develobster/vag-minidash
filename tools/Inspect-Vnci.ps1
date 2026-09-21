# Run in Windows PowerShell 5.1+ on the PC with working ODIS/VNCI drivers.
# Read-only inventory. Does not open VCI, scan network or send vehicle requests.
param([string]$Output = 'vnci-windows.json')
$ErrorActionPreference = 'Continue'
$issues = [System.Collections.Generic.List[string]]::new()
$modules = @()
Get-Process | Where-Object { $_.ProcessName -match 'ODIS|Offboard|DServer|Softing|VNCI' } | ForEach-Object {
    $process = $_
    try {
        $modules += @($process.Modules | Where-Object { $_.ModuleName -match 'pdu|vnci|vas|softing|pass|vci' } | ForEach-Object {
            [PSCustomObject]@{ Process=$process.ProcessName; PID=$process.Id; Module=$_.ModuleName; Path=$_.FileName; Version=$_.FileVersionInfo.FileVersion }
        })
    } catch { $issues.Add("Modules $($process.ProcessName): $($_.Exception.Message)") }
}
$registry = @()
foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32,[Microsoft.Win32.RegistryView]::Registry64)) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,$view)
    foreach ($name in @('SOFTWARE\PassThruSupport.04.04','SOFTWARE\PassThruSupport.05.00','SOFTWARE\D-PDU API','SOFTWARE\D-PDUAPI')) {
        $key = $base.OpenSubKey($name)
        if ($null -eq $key) { continue }
        try {
            $values = @{}; foreach ($v in $key.GetValueNames()) { $values[$v]=$key.GetValue($v) }
            $children = @(); foreach ($childName in $key.GetSubKeyNames()) {
                $child = $key.OpenSubKey($childName)
                try { $cv=@{}; foreach ($v in $child.GetValueNames()) {$cv[$v]=$child.GetValue($v)}; $children += @{Name=$childName;Values=$cv} }
                finally { $child.Close() }
            }
            $registry += @{View="$view";Path=$name;Values=$values;Children=$children}
        } finally { $key.Close() }
    }
    $base.Close()
}
$adapters = @(Get-NetAdapter -ErrorAction SilentlyContinue | Select-Object Name,InterfaceDescription,Status,LinkSpeed)
$addresses = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Select-Object InterfaceAlias,IPAddress,PrefixLength)
$connections = @(Get-NetTCPConnection -State Established -ErrorAction SilentlyContinue | Where-Object {
    $_.OwningProcess -in @($modules | Select-Object -ExpandProperty PID -Unique)
} | Select-Object LocalAddress,LocalPort,RemoteAddress,RemotePort,OwningProcess)
[ordered]@{CollectedAt=(Get-Date).ToUniversalTime().ToString('o'); VehicleIOPerformed=$false;
    Registry=$registry;LoadedModules=$modules;NetworkAdapters=$adapters;IPv4=$addresses;
    DiagnosticProcessConnections=$connections;Errors=@($issues);
    Note='No DLL execution. IP/Wi-Fi does not prove DoIP. Empty result is inconclusive; supply ODIS root XML or exact driver paths.'
} | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 $Output
Write-Host "Saved $Output"
