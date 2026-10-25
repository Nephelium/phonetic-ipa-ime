param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
try {
    . (Join-Path $PSScriptRoot 'Windows-Installation.ps1')
    $runtime = Join-Path (Split-Path -Parent $PSScriptRoot) 'output\runtime'
    if ($CheckOnly) { Assert-IpaRuntime $runtime; Write-Host '卸载预检通过，未修改系统。'; exit 0 }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    if (!([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        $child = Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe') -Verb RunAs -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"') -Wait -PassThru
        exit $child.ExitCode
    }
    Uninstall-IpaRuntime $runtime
    exit 0
} catch { Write-Host ('卸载失败：' + $_.Exception.Message); exit 1 }
