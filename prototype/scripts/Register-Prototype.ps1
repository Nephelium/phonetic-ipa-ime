# Run only after explicit approval to register this prototype in Windows.
# Requires an elevated PowerShell. No existing input method is removed or made the default.
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$runtime = Join-Path $taskRoot 'output\runtime'
$dll64 = Join-Path $runtime 'tsf-x64\PhoneticIpaTsf.dll'
$dll32 = Join-Path $runtime 'tsf-x86\PhoneticIpaTsf.dll'
$server = Join-Path $runtime 'PhoneticIpaServer.exe'
foreach ($file in @($dll64,$dll32,$server)) {
    if (!(Test-Path -LiteralPath $file)) { throw "Missing build artifact: $file" }
}
$regRoot = 'HKLM:\SOFTWARE\PhoneticIpa\Prototype'
New-Item -Path $regRoot -Force | Out-Null
New-ItemProperty -LiteralPath $regRoot -Name DataDir -Value (Join-Path $runtime 'data') -PropertyType String -Force | Out-Null
New-ItemProperty -LiteralPath $regRoot -Name ServerPath -Value $server -PropertyType String -Force | Out-Null
$reg64 = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" -ArgumentList '/s',"`"$dll64`"" -Wait -WindowStyle Hidden -PassThru
if ($reg64.ExitCode -ne 0) { throw "x64 registration failed: $($reg64.ExitCode)" }
$reg32 = Start-Process -FilePath "$env:SystemRoot\SysWOW64\regsvr32.exe" -ArgumentList '/s',"`"$dll32`"" -Wait -WindowStyle Hidden -PassThru
if ($reg32.ExitCode -ne 0) { throw "x86 registration failed: $($reg32.ExitCode)" }
'Registered 国际音标输入法原型. Select it from the Windows input method menu.'
