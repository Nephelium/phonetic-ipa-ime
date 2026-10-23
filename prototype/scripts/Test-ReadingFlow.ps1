param([string]$TestData = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
if (!$TestData) {
    $TestData = (Get-Content -LiteralPath "$taskRoot\output\headless-test-data-path.txt" -Encoding UTF8 -Raw).Trim()
}
$resolvedData = (Resolve-Path -LiteralPath $TestData).Path
if ($resolvedData -eq (Resolve-Path -LiteralPath "$taskRoot\output\runtime\data").Path) {
    throw 'Reading tests require an isolated data directory.'
}
foreach ($name in @('beijing-toned.tsv', 'word-readings.tsv', 'CC-CEDICT-README.txt', 'readings-provenance.json')) {
    Copy-Item -LiteralPath "$taskRoot\output\runtime\data\ipa\$name" -Destination "$resolvedData\ipa\$name"
}
if (Get-Process -Name PhoneticIpaServer -ErrorAction SilentlyContinue) {
    throw 'A prototype server is already running. Leave it untouched and run this check after it exits.'
}
$savedData = $env:METASEQUOIA_IME_DATA_DIR
$savedConfig = $env:METASEQUOIA_IME_CONFIG_DIR
$env:METASEQUOIA_IME_DATA_DIR = $resolvedData
$env:METASEQUOIA_IME_CONFIG_DIR = $resolvedData
$bin = "$taskRoot\output\build-server\bin\Release"
$ownedProcess = Start-Process -FilePath "$bin\PhoneticIpaServer.exe" -ArgumentList '--watchdog-managed --pipe-probe' -WorkingDirectory $bin -WindowStyle Hidden -PassThru
try {
    foreach ($arch in @('x64', 'Win32')) {
        & "$taskRoot\output\probe-$arch\Release\msime-ipc-probe.exe"
        if ($LASTEXITCODE -ne 0) { throw "Protocol probe failed: $arch" }
        & "$taskRoot\output\probe-$arch\Release\msime-ipc-probe.exe" --ipa-readings
        if ($LASTEXITCODE -ne 0) { throw "Reading flow probe failed: $arch" }
    }
} finally {
    if (!$ownedProcess.HasExited) {
        Stop-Process -Id $ownedProcess.Id
        $ownedProcess.WaitForExit(10000) | Out-Null
    }
    $env:METASEQUOIA_IME_DATA_DIR = $savedData
    $env:METASEQUOIA_IME_CONFIG_DIR = $savedConfig
}
