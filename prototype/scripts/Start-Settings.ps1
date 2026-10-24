param([switch]$Guide)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$runtime = Join-Path $taskRoot 'output\runtime'
$env:METASEQUOIA_IME_DATA_DIR = Join-Path $runtime 'data'
$env:METASEQUOIA_IME_CONFIG_DIR = $env:METASEQUOIA_IME_DATA_DIR
$executable = Join-Path $runtime 'PhoneticIpaSettings.exe'
if (!(Test-Path -LiteralPath $executable)) { throw 'Build and stage the prototype before opening settings.' }
# The user opens this launcher to interact with the settings window.
if ($Guide) {
    Start-Process -FilePath $executable -WorkingDirectory $runtime -ArgumentList '--guide' -WindowStyle Normal
} else {
    Start-Process -FilePath $executable -WorkingDirectory $runtime -WindowStyle Normal
}
