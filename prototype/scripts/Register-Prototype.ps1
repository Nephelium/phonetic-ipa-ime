#Requires -RunAsAdministrator
param([string]$UserDataDir = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Windows-Installation.ps1')
# Personal files of an earlier release are copied into the account that started the
# installation, which may differ from the administrator account approving it.
if (!$UserDataDir) { $UserDataDir = Join-Path $env:LOCALAPPDATA $script:IpaUserDataName }
if (![IO.Path]::IsPathRooted($UserDataDir) -or (Split-Path -Leaf $UserDataDir) -ne $script:IpaUserDataName) { throw 'Unexpected personal data folder.' }
$runtime = Join-Path (Split-Path -Parent $PSScriptRoot) 'output\runtime'
$target = Install-IpaRuntime $runtime $PSScriptRoot $UserDataDir
'安装完成，程序位于 ' + $target + '。旧版系统注册和后台进程已移除，个人数据已复制到 ' + $UserDataDir + '，旧版文件仍保留。'
