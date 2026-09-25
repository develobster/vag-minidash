[CmdletBinding()]
param(
    [string]$Port = 'COM4',
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$artifactDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\artifacts\hardware'))
[System.IO.Directory]::CreateDirectory($artifactDir) | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$baseName = "esp32s3-bringup-$stamp"
$logPath = Join-Path $artifactDir "$baseName.log"
$summaryPath = Join-Path $artifactDir "$baseName.json"
while ([System.IO.File]::Exists($logPath) -or [System.IO.File]::Exists($summaryPath)) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $baseName = "esp32s3-bringup-$stamp"
    $logPath = Join-Path $artifactDir "$baseName.log"
    $summaryPath = Join-Path $artifactDir "$baseName.json"
}

$utf8 = [System.Text.UTF8Encoding]::new($false)
$logStream = [System.IO.File]::Open($logPath, [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
$writer = [System.IO.StreamWriter]::new($logStream, $utf8)
$writer.AutoFlush = $true
$serial = $null
$boardTests = [ordered]@{}
$initialBoot = $null
$secondBoot = $null
$rebootSent = $false
$runSent = $false
$summarySeen = $false
$devicePass = $null
$deviceFail = $null
$errorMessage = $null
$rebootStatus = 'FAIL'
$rebootDetails = 'No software reboot was observed.'
$startedAt = [DateTime]::UtcNow
$timer = [System.Diagnostics.Stopwatch]::StartNew()

try {
    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200,
        [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.NewLine = "`n"
    $serial.ReadTimeout = 250
    $serial.WriteTimeout = 1000
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.Open()
    $serial.WriteLine('status')
    $writer.WriteLine('HOST_COMMAND status')

    Write-Host "Listening on $Port at 115200 baud. Evidence is being saved under artifacts\hardware\."
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        $line = $null
        try {
            $line = $serial.ReadLine().TrimEnd("`r")
        } catch [System.TimeoutException] {
            # Keep checking the total deadline while the serial port is quiet.
        }

        if ($null -ne $line -and $line.Length -gt 0) {
            $writer.WriteLine($line)
            Write-Host $line

            if ($line -match 'MINIDASH_BOOT count=(\d+) reset=([A-Z_]+)') {
                $boot = [pscustomobject]@{ Count = [int]$Matches[1]; Reset = $Matches[2] }
                if ($null -eq $initialBoot) {
                    $initialBoot = $boot
                } elseif ($rebootSent -and $null -eq $secondBoot) {
                    $secondBoot = $boot
                }
            }

            if ($line -match 'MINIDASH_TEST name=([^ ]+) status=(PASS|FAIL) details=(.*)') {
                $boardTests[$Matches[1]] = [pscustomobject]@{
                    status = $Matches[2]
                    details = $Matches[3]
                }
            }

            if ($line -match 'MINIDASH_SUMMARY pass=(\d+) fail=(\d+)') {
                $summarySeen = $true
                $devicePass = [int]$Matches[1]
                $deviceFail = [int]$Matches[2]
                if (-not $rebootSent) {
                    $serial.WriteLine('reboot')
                    $writer.WriteLine('HOST_COMMAND reboot')
                    $rebootSent = $true
                }
            }
        }

        if (-not $runSent -and $timer.Elapsed.TotalSeconds -ge 2) {
            $serial.WriteLine('run')
            $writer.WriteLine('HOST_COMMAND run')
            $runSent = $true
        }

        if ($rebootSent -and $null -ne $secondBoot -and
            $secondBoot.Reset -eq 'SW') {
            $rebootStatus = 'PASS'
            $rebootDetails = "software reboot observed; reset reason SW (boot count $($initialBoot.Count) to $($secondBoot.Count))."
            break
        }
    }

    if (-not $summarySeen) {
        throw "Timed out before firmware emitted MINIDASH_SUMMARY. Check that the bring-up firmware is running and USB Serial/JTAG is connected."
    }
    if ($rebootStatus -ne 'PASS') {
        throw 'The firmware tests finished, but the follow-up software reboot was not confirmed.'
    }
} catch {
    $errorMessage = $_.Exception.Message
    Write-Host "ERROR: $errorMessage"
} finally {
    if ($null -ne $serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
    $timer.Stop()
    $writer.Dispose()
}

$expectedTests = @(
    'chip_info', 'flash_size', 'psram_rw', 'internal_heap_rw',
    'gpio_driver', 'wifi_passive_scan', 'ble_controller',
    'task_watchdog_feed', 'usb_serial_jtag'
)
$missingTests = @($expectedTests | Where-Object { -not $boardTests.Contains($_) })
$failedTests = @($boardTests.Keys | Where-Object { $boardTests[$_].status -ne 'PASS' })
$overall = $null -eq $errorMessage -and $missingTests.Count -eq 0 -and
    $failedTests.Count -eq 0 -and $devicePass -eq $expectedTests.Count -and
    $deviceFail -eq 0 -and $rebootStatus -eq 'PASS'

$report = [ordered]@{
    started_utc = $startedAt.ToString('o')
    finished_utc = [DateTime]::UtcNow.ToString('o')
    port = $Port
    baud = 115200
    overall = if ($overall) { 'PASS' } else { 'FAIL' }
    device_reported_pass = $devicePass
    device_reported_fail = $deviceFail
    tests = $boardTests
    missing_tests = $missingTests
    failed_tests = $failedTests
    reboot_sanity = [ordered]@{ status = $rebootStatus; details = $rebootDetails }
    error = $errorMessage
    log = [System.IO.Path]::GetFileName($logPath)
}
[System.IO.File]::WriteAllText($summaryPath, ($report | ConvertTo-Json -Depth 6), $utf8)

Write-Host "Summary: $summaryPath"
Write-Host "Serial log: $logPath"
if (-not $overall) { exit 1 }
