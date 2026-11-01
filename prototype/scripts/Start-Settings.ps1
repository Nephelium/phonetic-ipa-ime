param([switch]$Guide)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$installed = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\PhoneticIpa\Prototype' -ErrorAction SilentlyContinue
if ($installed -and $installed.ServerPath -and (Test-Path -LiteralPath $installed.ServerPath -PathType Leaf)) {
    # Installed copy under Program Files: settings use this account's own data folder.
    $runtime = Split-Path -Parent $installed.ServerPath
    Remove-Item Env:PHONETIC_IPA_DATA_DIR, Env:PHONETIC_IPA_CONFIG_DIR -ErrorAction SilentlyContinue
} else {
    # Development run from the staged runtime.
    $runtime = Join-Path $taskRoot 'output\runtime'
    $env:PHONETIC_IPA_DATA_DIR = Join-Path $runtime 'data'
    $env:PHONETIC_IPA_CONFIG_DIR = $env:PHONETIC_IPA_DATA_DIR
}
$executable = Join-Path $runtime 'PhoneticIpaSettings.exe'
if (!(Test-Path -LiteralPath $executable)) { throw 'Build and stage the prototype before opening settings.' }
# The user opens this launcher to interact with the settings window.
if ($Guide) {
    Start-Process -FilePath $executable -WorkingDirectory $runtime -ArgumentList '--guide' -WindowStyle Normal
} else {
    Start-Process -FilePath $executable -WorkingDirectory $runtime -WindowStyle Normal
}
