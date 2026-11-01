$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\Windows-Installation.ps1')
$script:events = [Collections.Generic.List[string]]::new()
$script:failRegistration = $false
$script:brokenRuntime = ''
$script:target = 'C:\Program Files\PhoneticIPA\app-new'
$script:previous = [pscustomobject]@{ ServerPath='C:\old\PhoneticIpaServer.exe'; DataDir='C:\old\data' }
function Require($condition, $message) { if (!$condition) { throw $message } }
function Get-IpaInstallation { $script:previous }
function Assert-IpaRuntime([string]$Runtime) { $script:events.Add("validate:$Runtime") }
function Test-IpaRuntime([string]$Runtime) { $script:events.Add("check:$Runtime"); $Runtime -ne $script:brokenRuntime }
function Get-IpaProcesses { @() }
function Get-IpaLegacyDataSources { @('C:\old\data') }
function Stop-IpaProcesses([switch]$SkipValidation) { $script:events.Add($(if ($SkipValidation) { 'stop-any' } else { 'stop' })) }
function Copy-IpaPreferences($Sources, $Destination, [switch]$KeepNewerDestination) { $script:events.Add("preserve:$($Sources -join ',')>$Destination") }
function Copy-IpaProgram($Source, $ScriptDirectory, [switch]$SourceWasLive) { $script:events.Add("copy:$Source"); $script:target }
function Remove-IpaOldPrograms($Keep) { $script:events.Add("clean-old:$Keep") }
function Remove-IpaPath($Path) { $script:events.Add("delete:$Path") }
function Remove-IpaTipRegistration { $script:events.Add('remove-tip-keys') }
function Test-Path { param($LiteralPath); !([string]$LiteralPath).StartsWith('Registry::') }
function Invoke-IpaRegistration([string]$Runtime, [switch]$Unregister) {
    $action = if ($Unregister) { 'unregister' } else { 'register' }
    $script:events.Add("${action}:$Runtime")
    if ($script:failRegistration -and !$Unregister -and $Runtime -eq $script:target) { throw 'Simulated second-architecture failure' }
}
function Set-IpaInstallation($Runtime) { $script:events.Add("set:$Runtime") }
function New-Item { param($Path, [switch]$Force); $script:events.Add("new:$Path") }
function New-ItemProperty { param($LiteralPath,$Name,$Value,$PropertyType,[switch]$Force); $script:events.Add("property:$Name=$Value") }
function Remove-Item { param($LiteralPath,[switch]$Recurse,$ErrorAction); $script:events.Add("remove:$LiteralPath") }

$installed = Install-IpaRuntime 'C:\new' 'C:\new\scripts' 'C:\Users\me\AppData\Local\phonetic-ipa-prototype'
Require ($installed -eq $script:target) 'Install did not report the Program Files folder'
Require ($events.IndexOf('stop') -lt $events.IndexOf("unregister:$script:target")) 'Old processes were not stopped before unregistering'
Require ($events.IndexOf("unregister:$script:target") -lt $events.IndexOf("register:$script:target")) 'Upgrade did not unregister old product first'
Require ($events -contains "set:$script:target") 'Registration does not point at Program Files'
Require ($events -match 'property:UninstallString=.*Program Files\\PhoneticIPA\\scripts\\Uninstall-Windows.ps1') 'Uninstall entry still depends on the extraction folder'
Require ($events -contains 'preserve:C:\old\data>C:\Users\me\AppData\Local\phonetic-ipa-prototype') 'Earlier personal data was not copied to the starting account'
Require ($events.IndexOf('stop') -lt $events.IndexOf('preserve:C:\old\data>C:\Users\me\AppData\Local\phonetic-ipa-prototype')) 'Personal data copied while processes were running'
Require ($events -contains "clean-old:$script:target") 'Old program folders were not cleaned after success'
Write-Host 'PASS Program Files install, per-account data copy, upgrade order and uninstall entry'

$events.Clear(); $script:failRegistration = $true
try { Install-IpaRuntime 'C:\new' 'C:\new\scripts'; throw 'Failure not propagated' }
catch { Require ($_.Exception.Message -eq 'Simulated second-architecture failure') 'Wrong failure' }
Require ($events -contains 'property:ServerPath=C:\old\PhoneticIpaServer.exe') 'Failed upgrade did not restore old paths'
Require ($events -contains 'property:DataDir=C:\old\data') 'Failed upgrade did not restore the old data folder'
Require ($events -contains 'register:C:\old') 'Failed upgrade did not re-register old DLLs'
Require ($events -contains "delete:$script:target") 'Failed upgrade left its Program Files copy'
Require (!($events -like 'clean-old:*')) 'Failed upgrade removed older programs'
Write-Host 'PASS failed upgrade restores old registration'

$events.Clear(); $script:previous = $null
try { Install-IpaRuntime 'C:\new' 'C:\new\scripts'; throw 'Failure not propagated' }
catch { Require ($_.Exception.Message -eq 'Simulated second-architecture failure') 'Wrong failure' }
Require (($events | Where-Object { $_ -eq "unregister:$script:target" }).Count -eq 2) 'Partial new registration was not rolled back'
Require ($events -contains "remove:$script:IpaRegistry") 'Failed first install left registry paths'
Write-Host 'PASS failed first installation removes partial registration'

$events.Clear(); $script:failRegistration = $false
$script:previous = [pscustomobject]@{ ServerPath="$script:target\PhoneticIpaServer.exe"; FactoryDataDir="$script:target\data" }
Uninstall-IpaRuntime
Require ($events -contains 'stop-any') 'Uninstaller must stop even a damaged installation'
Require ($events -contains "unregister:$script:target") 'Uninstaller did not remove TSF registration'
Require ($events -contains 'clean-old:') 'Uninstaller left program folders'
Require (($events | Where-Object { $_ -like 'remove:*' -and $_ -notlike 'remove:HKLM:*' }).Count -eq 0) 'Uninstaller deleted user files'
Write-Host 'PASS uninstall removes product registration and programs, retains user files'

$events.Clear(); $script:brokenRuntime = $script:target
Uninstall-IpaRuntime
Require ($events -contains 'remove-tip-keys') 'Changed program files were not unregistered by key removal'
Require (!($events -contains "unregister:$script:target")) 'Uninstaller ran code from a changed DLL'
Write-Host 'PASS uninstall of a changed installation removes registration without running its DLL'
