$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$runtime = Join-Path $taskRoot 'output\runtime'
$env:PHONETIC_IPA_DATA_DIR = Join-Path $runtime 'data'
$env:PHONETIC_IPA_CONFIG_DIR = $env:PHONETIC_IPA_DATA_DIR
$process = Start-Process -FilePath (Join-Path $runtime 'PhoneticIpaServer.exe') -ArgumentList '--watchdog-managed' -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
$process | Select-Object Id,ProcessName
