$script:IpaRegistry = 'HKLM:\SOFTWARE\PhoneticIpa\Prototype'
$script:IpaUninstallRegistry = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PhoneticIPA'
$script:IpaClsid = '{D95EFABF-8AEC-5396-AEA4-A4E47D5F21BF}'

function Get-IpaInstallation {
    Get-ItemProperty -LiteralPath $script:IpaRegistry -ErrorAction SilentlyContinue
}
function Assert-IpaRuntime([string]$Runtime) {
    foreach ($relative in @('PhoneticIpaServer.exe', 'PhoneticIpaSettings.exe', 'PhoneticIpaWatchdog.exe', 'tsf-x64\PhoneticIpaTsf.dll', 'tsf-x86\PhoneticIpaTsf.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $Runtime $relative) -PathType Leaf)) { throw "安装文件不完整：$relative" }
    }
    $manifest = Get-Content -LiteralPath (Join-Path $Runtime 'build-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($entry in $manifest.PSObject.Properties) {
        $file = [IO.Path]::GetFullPath((Join-Path $Runtime $entry.Name))
        if (!$file.StartsWith([IO.Path]::GetFullPath($Runtime).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid runtime manifest path.' }
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.Value) { throw "安装文件校验失败：$($entry.Name)" }
    }
}
function Get-IpaProcesses {
    @(Get-CimInstance Win32_Process -Filter "Name = 'PhoneticIpaWatchdog.exe' OR Name = 'PhoneticIpaServer.exe' OR Name = 'PhoneticIpaSettings.exe'")
}
function Stop-IpaProcesses {
    # Validate each exact runtime before stopping it. This also handles an old
    # watchdog surviving a prior installer that changed only the registry.
    $processes = @(Get-IpaProcesses)
    $roots = @{}
    foreach ($process in $processes) {
        if (!$process.ExecutablePath) { throw '无法确认输入法进程路径，请关闭输入法设置后重试。' }
        $root = Split-Path -Parent $process.ExecutablePath
        if (!$roots.ContainsKey($root)) { Assert-IpaRuntime $root; $roots[$root] = $true }
    }
    foreach ($name in @('PhoneticIpaWatchdog.exe', 'PhoneticIpaSettings.exe', 'PhoneticIpaServer.exe')) {
        foreach ($process in $processes | Where-Object Name -eq $name) {
            Stop-Process -Id $process.ProcessId -Force -ErrorAction Stop
            Wait-Process -Id $process.ProcessId -Timeout 10 -ErrorAction SilentlyContinue
        }
    }
    if (@(Get-IpaProcesses).Count) { throw '输入法进程仍在运行，请稍后重试。' }
}
function Invoke-IpaRegistration([string]$Runtime, [switch]$Unregister) {
    foreach ($arch in @('x64', 'x86')) {
        $system = if ($arch -eq 'x64') { 'System32' } else { 'SysWOW64' }
        $dll = Join-Path $Runtime "tsf-$arch\PhoneticIpaTsf.dll"
        $arguments = @('/s'); if ($Unregister) { $arguments += '/u' }; $arguments += ('"' + $dll + '"')
        $child = Start-Process -FilePath (Join-Path $env:SystemRoot "$system\regsvr32.exe") -ArgumentList $arguments -Wait -WindowStyle Hidden -PassThru
        if ($child.ExitCode -ne 0) { throw "输入法 $arch 注册操作失败，退出码 $($child.ExitCode)" }
        $view = if ($arch -eq 'x64') { 'Registry64' } else { 'Registry32' }
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::$view)
        try {
            $com = $base.OpenSubKey("SOFTWARE\Classes\CLSID\$script:IpaClsid\InProcServer32")
            $profile = $base.OpenSubKey("SOFTWARE\Microsoft\CTF\TIP\$script:IpaClsid\LanguageProfile\0x00000804\{881BB948-8042-590E-AAC7-77D38BAFB1E6}")
            try {
                if ($Unregister) {
                    if ($com -or $profile) { throw "输入法 $arch 卸载注册未完成，请重试。" }
                } elseif (!$com -or !$profile -or $com.GetValue('') -ne $dll) {
                    throw "输入法 $arch 注册结果与安装目录不一致。"
                }
            } finally { if ($com) { $com.Dispose() }; if ($profile) { $profile.Dispose() } }
        } finally { $base.Dispose() }
    }
}
function Set-IpaInstallation([string]$Runtime, [string]$DataDir) {
    New-Item -Path $script:IpaRegistry -Force | Out-Null
    New-ItemProperty -LiteralPath $script:IpaRegistry -Name DataDir -Value $DataDir -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $script:IpaRegistry -Name ServerPath -Value (Join-Path $Runtime 'PhoneticIpaServer.exe') -PropertyType String -Force | Out-Null
}
function Copy-IpaPreferences([string[]]$Sources, [string]$Destination) {
    $Sources = @($Sources | Where-Object { $_ -and $_ -ne $Destination -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)
    if (!$Sources.Count) { return }
    # Preserve user-created files. Factory catalog/font resources always come
    # from the new package. Old directories and databases stay available.
    $files = @('config.toml', 'msime_user.db', 'msime_user.db-wal', 'msime_user.db-shm', 'user_dict.dat', 'ipa\active.cin', 'ipa\rule-selection.txt', 'ipa\custom-icon-name.txt')
    foreach ($Source in $Sources) {
        $directory = 'ipa'
        $path = Join-Path $Source $directory
        if (Test-Path -LiteralPath $path) {
            $files += @(Get-ChildItem -LiteralPath $path -File | Where-Object { $_.Name -match '^(user|custom|onboarding|update)' } | ForEach-Object { "$directory\$($_.Name)" })
        }
    }
    foreach ($relative in $files | Select-Object -Unique) {
        $candidates = @($Sources | ForEach-Object { Get-Item -LiteralPath (Join-Path $_ $relative) -ErrorAction SilentlyContinue } | Where-Object { !$_.PSIsContainer } | Sort-Object LastWriteTimeUtc -Descending)
        $to = Join-Path $Destination $relative
        if ($candidates.Count) {
            $from = $candidates[0].FullName
            New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
            if (Test-Path -LiteralPath $to) { Copy-Item -LiteralPath $to -Destination ($to + '.before-upgrade-' + (Get-Date -Format 'yyyyMMddHHmmssfff')) }
            Copy-Item -LiteralPath $from -Destination $to -Force
        }
    }
}
function Install-IpaRuntime([string]$Runtime, [string]$ScriptDirectory) {
    Assert-IpaRuntime $Runtime
    $previous = Get-IpaInstallation
    $oldRuntime = if ($previous -and $previous.ServerPath) { Split-Path -Parent $previous.ServerPath } else { $null }
    if ($oldRuntime -and (Test-Path -LiteralPath $oldRuntime)) { Assert-IpaRuntime $oldRuntime }
    $dataSources = @()
    if ($previous) { $dataSources += $previous.DataDir }
    $dataSources += @(Get-IpaProcesses | ForEach-Object { Join-Path (Split-Path -Parent $_.ExecutablePath) 'data' })
    Stop-IpaProcesses
    Copy-IpaPreferences $dataSources (Join-Path $Runtime 'data')
    try {
        # The new DLL has the same independent CLSID, so it can also remove a
        # stale registration whose old extraction directory has disappeared.
        Invoke-IpaRegistration $Runtime -Unregister
        Set-IpaInstallation $Runtime (Join-Path $Runtime 'data')
        Invoke-IpaRegistration $Runtime
        New-Item -Path $script:IpaUninstallRegistry -Force | Out-Null
        $powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $command = '"' + $powershell + '" -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $ScriptDirectory 'Uninstall-Windows.ps1') + '"'
        foreach ($pair in @(@('DisplayName','国际音标输入法'), @('DisplayVersion','0.1.7'), @('Publisher','Nephelium'), @('InstallLocation',$Runtime), @('UninstallString',$command), @('DisplayIcon',(Join-Path $Runtime 'PhoneticIpaSettings.exe')))) {
            New-ItemProperty -LiteralPath $script:IpaUninstallRegistry -Name $pair[0] -Value $pair[1] -PropertyType String -Force | Out-Null
        }
        foreach ($name in @('NoModify','NoRepair')) { New-ItemProperty -LiteralPath $script:IpaUninstallRegistry -Name $name -Value 1 -PropertyType DWord -Force | Out-Null }
    } catch {
        $failure = $_
        if ($oldRuntime -and (Test-Path -LiteralPath $oldRuntime)) {
            Set-IpaInstallation $oldRuntime $previous.DataDir
            Invoke-IpaRegistration $oldRuntime
        } else {
            Invoke-IpaRegistration $Runtime -Unregister
            Remove-Item -LiteralPath $script:IpaRegistry -Recurse -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $script:IpaUninstallRegistry -Recurse -ErrorAction SilentlyContinue
        }
        throw $failure
    }
}
function Uninstall-IpaRuntime([string]$FallbackRuntime) {
    $installed = Get-IpaInstallation
    $runtime = if ($installed -and $installed.ServerPath) { Split-Path -Parent $installed.ServerPath } else { $FallbackRuntime }
    if (!(Test-Path -LiteralPath (Join-Path $runtime 'tsf-x64\PhoneticIpaTsf.dll'))) { $runtime = $FallbackRuntime }
    Assert-IpaRuntime $runtime
    Stop-IpaProcesses
    Invoke-IpaRegistration $runtime -Unregister
    foreach ($path in @("Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\$script:IpaClsid", "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\WOW6432Node\CLSID\$script:IpaClsid")) {
        if (Test-Path -LiteralPath $path) { throw '卸载注册未完成，已保留卸载入口，请重试。' }
    }
    Remove-Item -LiteralPath $script:IpaRegistry -Recurse -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $script:IpaUninstallRegistry -Recurse -ErrorAction SilentlyContinue
    Write-Host '国际音标输入法已卸载。个人键位、配置和解压文件已保留，可随时重新安装。'
}
