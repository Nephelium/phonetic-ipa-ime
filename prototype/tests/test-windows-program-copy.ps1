$ErrorActionPreference = 'Stop'
# Real-file checks for the Program Files layout (2026-11-01 review H1/H2) inside a scratch folder.
. (Join-Path $PSScriptRoot '..\scripts\Windows-Installation.ps1')
function Require($condition, $message) { if (!$condition) { throw $message } }
$testRoot = Join-Path (Split-Path -Parent $PSScriptRoot) ('output\install-layout-tests\' + [Guid]::NewGuid().ToString('N'))
$script:IpaProgramRoot = Join-Path $testRoot 'ProgramFiles\PhoneticIPA'
$source = Join-Path $testRoot 'extracted\output\runtime'
$scripts = Join-Path $testRoot 'extracted\scripts'
foreach ($folder in @($source, (Join-Path $source 'tsf-x64'), (Join-Path $source 'tsf-x86'), (Join-Path $source 'data\ipa'), $scripts)) {
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
}
$manifest = [ordered]@{}
foreach ($binary in @('PhoneticIpaServer.exe', 'PhoneticIpaSettings.exe', 'PhoneticIpaWatchdog.exe', 'tsf-x64\PhoneticIpaTsf.dll', 'tsf-x86\PhoneticIpaTsf.dll')) {
    Set-Content -LiteralPath (Join-Path $source $binary) -Value $binary -Encoding ASCII
    $manifest[$binary] = (Get-FileHash -LiteralPath (Join-Path $source $binary) -Algorithm SHA256).Hash
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $source 'build-manifest.json') -Encoding UTF8
foreach ($pair in @(@('data\config.toml', 'factory-config'), @('data\ipa\default.cin', 'factory-keys'), @('data\ipa\active.cin', 'someone-elses-keys'),
                    @('data\msime_user.db', 'learned'), @('data\ipa\onboarding-v2.done', '1'), @('data\ipa\active.cin.backup-1-2-3', 'old'))) {
    Set-Content -LiteralPath (Join-Path $source $pair[0]) -Value $pair[1] -Encoding UTF8
}
foreach ($name in @('Windows-Installation.ps1', 'Uninstall-Windows.ps1')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot "..\scripts\$name") -Destination $scripts }

$fresh = Copy-IpaProgram $source $scripts
Require (Test-IpaUnderProgramRoot $fresh) 'Program copy left Program Files'
Require (Test-Path -LiteralPath (Join-Path $fresh 'data\ipa\default.cin')) 'Factory resource missing'
Require ((Get-Content -LiteralPath (Join-Path $fresh 'data\config.toml') -Raw).Trim() -eq 'factory-config') 'Factory config missing from a fresh package'
foreach ($personal in @('data\ipa\active.cin', 'data\msime_user.db', 'data\ipa\onboarding-v2.done', 'data\ipa\active.cin.backup-1-2-3')) {
    Require (!(Test-Path -LiteralPath (Join-Path $fresh $personal))) "Personal file shared through factory data: $personal"
}
Require (Test-Path -LiteralPath (Join-Path $fresh 'data\factory-stamp.txt')) 'Factory stamp missing'
Require (Test-Path -LiteralPath (Join-Path $script:IpaProgramRoot 'scripts\Uninstall-Windows.ps1')) 'Uninstaller not copied to Program Files'
Write-Host 'PASS factory copy excludes personal files and keeps the uninstaller outside the extraction folder'

Start-Sleep -Seconds 1
$live = Copy-IpaProgram $source $scripts -SourceWasLive
Require (!(Test-Path -LiteralPath (Join-Path $live 'data\config.toml'))) 'A live data folder config.toml became the factory default'
$foreign = Join-Path $script:IpaProgramRoot 'app-unmarked'
New-Item -ItemType Directory -Path $foreign -Force | Out-Null
# An interrupted earlier removal can leave an installer-named folder without its marker.
$interrupted = Join-Path (Join-Path $script:IpaProgramRoot 'app-0.1.6-20200101000000') 'tsf-x64'
New-Item -ItemType Directory -Path $interrupted -Force | Out-Null
Set-Content -LiteralPath (Join-Path $interrupted 'PhoneticIpaTsf.dll') -Value 'old' -Encoding ASCII
Remove-IpaOldPrograms $live
Require (!(Test-Path -LiteralPath (Split-Path -Parent $interrupted))) 'Unmarked installer-named folder was left behind'
Require (!(Test-Path -LiteralPath $fresh)) 'Older marked program folder was not deleted'
Require (Test-Path -LiteralPath $live) 'Current program folder was deleted'
Require (Test-Path -LiteralPath $foreign) 'Unmarked folder was deleted'
Require (Test-Path -LiteralPath (Join-Path $source 'data\ipa\active.cin')) 'Extraction folder was touched'
Write-Host 'PASS old program cleanup deletes only marked folders and never the extraction folder'

$account = Join-Path $testRoot 'LocalAppData\phonetic-ipa-prototype'
New-Item -ItemType Directory -Path (Join-Path $account 'ipa') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $account 'ipa\rule-selection.txt') -Value 'newer-choice' -Encoding UTF8
(Get-Item -LiteralPath (Join-Path $source 'data\ipa\active.cin')).LastWriteTimeUtc = [DateTime]::UtcNow.AddDays(-1)
Set-Content -LiteralPath (Join-Path $source 'data\ipa\rule-selection.txt') -Value 'older-choice' -Encoding UTF8
(Get-Item -LiteralPath (Join-Path $source 'data\ipa\rule-selection.txt')).LastWriteTimeUtc = [DateTime]::UtcNow.AddDays(-2)
Copy-IpaPreferences @((Join-Path $source 'data')) $account -KeepNewerDestination
Require ((Get-Content -LiteralPath (Join-Path $account 'ipa\active.cin') -Raw).Trim() -eq 'someone-elses-keys') 'Earlier keymap not migrated'
Require ((Get-Content -LiteralPath (Join-Path $account 'ipa\rule-selection.txt') -Raw).Trim() -eq 'newer-choice') 'Migration replaced a newer personal file'
Write-Host 'PASS migration copies earlier personal files and keeps newer ones'

$uninstall = Join-Path $script:IpaProgramRoot 'scripts'
Remove-IpaPath $uninstall
Require (!(Test-Path -LiteralPath $uninstall)) 'Program scripts folder not removed'
Remove-IpaPath $source
Require (Test-Path -LiteralPath $source) 'Remove-IpaPath deleted a folder outside Program Files'
Write-Host 'PASS removal is limited to Program Files'
Remove-Item -LiteralPath $testRoot -Recurse -Force
