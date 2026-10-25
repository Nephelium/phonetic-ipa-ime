param([switch]$CheckOnly, [string]$LogFile = '')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskLogs = Join-Path $taskRoot 'install-logs'
$taskResult = 0
function Write-InstallLog([string]$Message) {
    Write-Host $Message
    if ($script:taskLogFile) {
        try { Add-Content -LiteralPath $script:taskLogFile -Value ((Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ' + $Message) -Encoding UTF8 } catch { Write-Host ('Log write failed: ' + $_.Exception.Message) }
    }
}
try {
    New-Item -ItemType Directory -Path $taskLogs -Force | Out-Null
    if ($LogFile) {
        $taskResolvedLog = [IO.Path]::GetFullPath($LogFile)
        if (!$taskResolvedLog.StartsWith([IO.Path]::GetFullPath($taskLogs) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected installer log path.' }
        $script:taskLogFile = $taskResolvedLog
    } else {
        $script:taskLogFile = Join-Path $taskLogs ('install-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + $PID + '.log')
    }
    Write-InstallLog ('Windows installer r3 / PowerShell ' + $PSVersionTable.PSVersion)
    foreach ($taskRelative in @('scripts\Windows-Installation.ps1', 'scripts\Uninstall-Windows.ps1', 'scripts\Register-Prototype.ps1', 'scripts\Start-Settings.ps1', 'output\runtime\PhoneticIpaServer.exe', 'output\runtime\PhoneticIpaSettings.exe', 'output\runtime\tsf-x64\PhoneticIpaTsf.dll', 'output\runtime\tsf-x86\PhoneticIpaTsf.dll', 'output\runtime\data\config.toml')) {
        if (!(Test-Path -LiteralPath (Join-Path $taskRoot $taskRelative) -PathType Leaf)) { throw ('Missing package file: ' + $taskRelative + '. Extract the complete ZIP first.') }
    }
    foreach ($taskSkin in @('bigfish', 'bigfish-peek', 'bigfish-national-day')) {
        foreach ($taskAsset in @('skin.toml', 'assets\background.png', 'assets\character.png')) {
            $taskSkinFile = Join-Path $taskRoot ('output\runtime\data\skins\' + $taskSkin + '\' + $taskAsset)
            if (!(Test-Path -LiteralPath $taskSkinFile -PathType Leaf)) { throw ('Missing package skin file: ' + $taskSkin + '\' + $taskAsset + '. Extract the complete ZIP first.') }
        }
    }
    . (Join-Path $PSScriptRoot 'Windows-Installation.ps1')
    Assert-IpaRuntime (Join-Path $taskRoot 'output\runtime')
    if ($CheckOnly) {
        Write-InstallLog 'Preflight passed. No system registration was performed.'
    } else {
        $taskIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $taskAdministrator = ([Security.Principal.WindowsPrincipal]::new($taskIdentity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
        if (!$taskAdministrator) {
            Write-InstallLog '请在管理员授权窗口中确认安装。'
            $taskArguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -LogFile "' + $script:taskLogFile + '"'
            $taskChild = Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe') -Verb RunAs -ArgumentList $taskArguments -Wait -PassThru
            $taskResult = $taskChild.ExitCode
            if ($taskResult -ne 0) { Write-Host (Get-Content -LiteralPath $script:taskLogFile -Raw -Encoding UTF8); throw ('安装未完成，退出码 ' + $taskResult) }
            try { & (Join-Path $PSScriptRoot 'Start-Settings.ps1') -Guide }
            catch { Write-InstallLog '安装已完成。教学窗口未能打开，请双击设置启动器查看使用说明。' }
            Write-InstallLog '安装完成。请按 Win + 空格选择国际音标输入法原型。'
            Write-InstallLog '升级后请保存草稿，并完全退出再打开仍在运行的应用，让它们加载新版输入法。不需要重启电脑。'
        } else {
            & (Join-Path $PSScriptRoot 'Register-Prototype.ps1') | ForEach-Object { Write-InstallLog ([string]$_) }
            if (!$LogFile) {
                try { & (Join-Path $PSScriptRoot 'Start-Settings.ps1') -Guide }
                catch { Write-InstallLog '安装已完成。请双击使用教学启动器查看说明。' }
            }
            Write-InstallLog '安装完成。请按 Win + 空格选择国际音标输入法原型。'
            Write-InstallLog '升级后请保存草稿，并完全退出再打开仍在运行的应用，让它们加载新版输入法。不需要重启电脑。'
        }
    }
} catch {
    $taskResult = 1
    Write-InstallLog ('安装失败：' + $_.Exception.Message)
    Write-InstallLog ([string]$_.InvocationInfo.PositionMessage)
} finally {
    if ($script:taskLogFile) { Write-Host ('安装日志：' + $script:taskLogFile) }
}
exit $taskResult
