$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$administrator = [Security.Principal.WindowsPrincipal]::new($identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$administrator) {
    Start-Process powershell.exe -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"") -Wait
    exit
}
try {
    & (Join-Path $PSScriptRoot 'Register-Prototype.ps1')
    Write-Host '安装完成。请按 Win + 空格选择国际音标输入法原型。'
} catch {
    Write-Error $_ -ErrorAction Continue
} finally {
    Read-Host '按回车关闭安装窗口'
}
