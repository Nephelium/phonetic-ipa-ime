$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\Windows-Installation.ps1')
$testRoot = Join-Path (Split-Path -Parent $PSScriptRoot) ('output\ipa-plus-fix-20261025\preferences-' + [Guid]::NewGuid().ToString('N'))
foreach ($folder in @('old', 'running', 'new')) {
    New-Item -ItemType Directory -Path (Join-Path $testRoot "$folder\ipa") -Force | Out-Null
}
$old = Join-Path $testRoot 'old'
$running = Join-Path $testRoot 'running'
$destination = Join-Path $testRoot 'new'
Set-Content -LiteralPath (Join-Path $old 'config.toml') -Value 'old' -Encoding UTF8
(Get-Item -LiteralPath (Join-Path $old 'config.toml')).LastWriteTimeUtc = [DateTime]::UtcNow.AddDays(-1)
Set-Content -LiteralPath (Join-Path $running 'config.toml') -Value 'latest' -Encoding UTF8
Set-Content -LiteralPath (Join-Path $destination 'config.toml') -Value 'factory' -Encoding UTF8
Set-Content -LiteralPath (Join-Path $old 'ipa\active.cin') -Value 'custom-keymap' -Encoding UTF8
Set-Content -LiteralPath (Join-Path $old 'ipa\plus-catalog.json') -Value 'old-catalog' -Encoding UTF8
Set-Content -LiteralPath (Join-Path $destination 'ipa\plus-catalog.json') -Value 'new-catalog' -Encoding UTF8
Copy-IpaPreferences @($old, $running) $destination
if ((Get-Content -LiteralPath (Join-Path $destination 'config.toml') -Raw -Encoding UTF8).Trim() -ne 'latest') { throw 'Newest preferences not selected' }
if ((Get-Content -LiteralPath (Join-Path $destination 'ipa\active.cin') -Raw -Encoding UTF8).Trim() -ne 'custom-keymap') { throw 'Custom keymap lost' }
if ((Get-Content -LiteralPath (Join-Path $destination 'ipa\plus-catalog.json') -Raw -Encoding UTF8).Trim() -ne 'new-catalog') { throw 'New factory symbols overwritten' }
if (@(Get-ChildItem -LiteralPath $destination -Filter 'config.toml.before-upgrade-*').Count -ne 1) { throw 'Destination backup missing' }
if (!(Test-Path -LiteralPath (Join-Path $old 'config.toml'))) { throw 'Old preferences removed' }
Write-Host 'PASS newest preferences, keymap preservation, destination backup and new catalog retention'
