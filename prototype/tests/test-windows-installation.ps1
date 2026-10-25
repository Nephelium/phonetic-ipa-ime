$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\scripts\Windows-Installation.ps1')
$script:events = [Collections.Generic.List[string]]::new()
$script:failRegistration = $false
$script:previous = [pscustomobject]@{ ServerPath='C:\old\PhoneticIpaServer.exe'; DataDir='C:\old\data' }
function Require($condition, $message) { if (!$condition) { throw $message } }
function Get-IpaInstallation { $script:previous }
function Assert-IpaRuntime([string]$Runtime) { $script:events.Add("validate:$Runtime") }
function Get-IpaProcesses { @() }
function Stop-IpaProcesses { $script:events.Add('stop') }
function Copy-IpaPreferences($Sources, $Destination) { $script:events.Add('preserve') }
function Test-Path { param($LiteralPath); !([string]$LiteralPath).StartsWith('Registry::') }
function Invoke-IpaRegistration([string]$Runtime, [switch]$Unregister) {
    $action = if ($Unregister) { 'unregister' } else { 'register' }
    $script:events.Add("${action}:$Runtime")
    if ($script:failRegistration -and !$Unregister -and $Runtime -eq 'C:\new') { throw 'Simulated second-architecture failure' }
}
function Set-IpaInstallation($Runtime, $DataDir) { $script:events.Add("set:$Runtime") }
function New-Item { param($Path, [switch]$Force); $script:events.Add("new:$Path") }
function New-ItemProperty { param($LiteralPath,$Name,$Value,$PropertyType,[switch]$Force); $script:events.Add("property:$Name=$Value") }
function Remove-Item { param($LiteralPath,[switch]$Recurse,$ErrorAction); $script:events.Add("remove:$LiteralPath") }
Install-IpaRuntime 'C:\new' 'C:\new\scripts'
Require ($events.IndexOf('stop') -lt $events.IndexOf('unregister:C:\new')) 'Old processes were not stopped before unregistering'
Require ($events.IndexOf('unregister:C:\new') -lt $events.IndexOf('register:C:\new')) 'Upgrade did not unregister old product first'
Require ($events -match 'property:UninstallString=.*Uninstall-Windows.ps1') 'Missing Windows uninstall entry'
Require ($events -contains 'preserve') 'Preferences were not preserved'
Write-Host 'PASS upgrade order, preservation and uninstall entry'
$events.Clear(); $script:failRegistration = $true
try { Install-IpaRuntime 'C:\new' 'C:\new\scripts'; throw 'Failure not propagated' }
catch { Require ($_.Exception.Message -eq 'Simulated second-architecture failure') 'Wrong failure' }
Require ($events -contains 'set:C:\old') 'Failed upgrade did not restore old paths'
Require ($events -contains 'register:C:\old') 'Failed upgrade did not re-register old DLLs'
Write-Host 'PASS failed upgrade restores old registration'
$events.Clear(); $script:previous = $null
try { Install-IpaRuntime 'C:\new' 'C:\new\scripts'; throw 'Failure not propagated' }
catch { Require ($_.Exception.Message -eq 'Simulated second-architecture failure') 'Wrong failure' }
Require (($events | Where-Object { $_ -eq 'unregister:C:\new' }).Count -eq 2) 'Partial new registration was not rolled back'
Require ($events -contains "remove:$script:IpaRegistry") 'Failed first install left registry paths'
Write-Host 'PASS failed first installation removes partial registration'
$events.Clear(); $script:failRegistration = $false
Uninstall-IpaRuntime 'C:\new'
Require ($events -contains 'unregister:C:\new') 'Uninstaller did not remove TSF registration'
Require (($events | Where-Object { $_ -like 'remove:*' -and $_ -notlike 'remove:HKLM:*' }).Count -eq 0) 'Uninstaller deleted user files'
Write-Host 'PASS uninstall removes product registration and retains user files'
