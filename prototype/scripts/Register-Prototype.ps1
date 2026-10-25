#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Windows-Installation.ps1')
$runtime = Join-Path (Split-Path -Parent $PSScriptRoot) 'output\runtime'
Install-IpaRuntime $runtime $PSScriptRoot
'安装完成，旧版系统注册和后台进程已移除。个人数据与旧版文件已保留。'
