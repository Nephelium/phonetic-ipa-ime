$script:IpaRegistry = 'HKLM:\SOFTWARE\PhoneticIpa\Prototype'
$script:IpaUninstallRegistry = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PhoneticIPA'
$script:IpaClsid = '{D95EFABF-8AEC-5396-AEA4-A4E47D5F21BF}'
# Programs and factory resources are copied under Program Files, which only administrators can
# change, and registered there (2026-11-01 review H1). Personal data lives in each account's
# %LOCALAPPDATA%\phonetic-ipa-prototype (H2); the Server seeds it from the factory directory.
$script:IpaProgramRoot = Join-Path $env:ProgramFiles 'PhoneticIPA'
$script:IpaUserDataName = 'phonetic-ipa-prototype'
$script:IpaVersion = '0.1.8' # kept in step with msime-windows/version.txt by scripts/sync-version.py
$script:IpaAppMarker = '.phonetic-ipa-app'
$script:IpaPersonalFiles = @('config.toml', 'msime_user.db', 'msime_user.db-wal', 'msime_user.db-shm', 'user_dict.dat', 'ipa\active.cin', 'ipa\rule-selection.txt', 'ipa\custom-icon-name.txt')

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
function Test-IpaRuntime([string]$Runtime) {
    try { Assert-IpaRuntime $Runtime; $true } catch { $false }
}
function Test-IpaUnderProgramRoot([string]$Path) {
    if (!$Path) { return $false }
    $root = [IO.Path]::GetFullPath($script:IpaProgramRoot).TrimEnd('\') + '\'
    [IO.Path]::GetFullPath($Path).StartsWith($root, [StringComparison]::OrdinalIgnoreCase)
}
# Mirrors PhoneticIpa::UserData::IsPersonal in server/src/ipa/ipa_user_data.h.
function Test-IpaPersonalFile([string]$Relative) {
    $name = Split-Path -Leaf $Relative
    if ($script:IpaPersonalFiles -contains $Relative -or $Relative -eq 'ipa\active.cin.previous') { return $true }
    if ($name -like 'msime_user.db*') { return $true }
    if ($Relative -like 'ipa\*' -and $name -match '^(user|custom|onboarding|update)') { return $true }
    $name -match '\.(backup|pending|before-upgrade)-'
}
function Get-IpaProcesses {
    @(Get-CimInstance Win32_Process -Filter "Name = 'PhoneticIpaWatchdog.exe' OR Name = 'PhoneticIpaServer.exe' OR Name = 'PhoneticIpaSettings.exe'")
}
function Stop-IpaProcesses([switch]$SkipValidation) {
    # Validate each exact runtime before stopping it. This also handles an old
    # watchdog surviving a prior installer that changed only the registry.
    # Uninstall skips validation so a damaged installation can still be removed.
    $processes = @(Get-IpaProcesses)
    $roots = @{}
    foreach ($process in $processes) {
        if (!$process.ExecutablePath) { throw '无法确认输入法进程路径，请关闭输入法设置后重试。' }
        $root = Split-Path -Parent $process.ExecutablePath
        if (!$SkipValidation -and !$roots.ContainsKey($root)) { Assert-IpaRuntime $root; $roots[$root] = $true }
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
# Used when the registered DLL is missing or fails verification: deleting the COM and TSF keys
# directly avoids running code from a DLL that may have been changed (2026-11-01 review L5).
function Remove-IpaTipRegistration {
    foreach ($view in @('Registry64', 'Registry32')) {
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::$view)
        try {
            foreach ($key in @("SOFTWARE\Classes\CLSID\$script:IpaClsid", "SOFTWARE\Microsoft\CTF\TIP\$script:IpaClsid")) {
                $existing = $base.OpenSubKey($key)
                if ($existing) { $existing.Dispose(); $base.DeleteSubKeyTree($key, $false) }
            }
        } finally { $base.Dispose() }
    }
}
function Set-IpaInstallation([string]$Runtime) {
    New-Item -Path $script:IpaRegistry -Force | Out-Null
    # DataDir pointed every account at one shared folder. Only the read-only factory folder is
    # recorded now; personal data follows each account's LOCALAPPDATA.
    Remove-ItemProperty -LiteralPath $script:IpaRegistry -Name DataDir -ErrorAction SilentlyContinue
    New-ItemProperty -LiteralPath $script:IpaRegistry -Name FactoryDataDir -Value (Join-Path $Runtime 'data') -PropertyType String -Force | Out-Null
    New-ItemProperty -LiteralPath $script:IpaRegistry -Name ServerPath -Value (Join-Path $Runtime 'PhoneticIpaServer.exe') -PropertyType String -Force | Out-Null
}
function Copy-IpaPreferences([string[]]$Sources, [string]$Destination, [switch]$KeepNewerDestination) {
    $Sources = @($Sources | Where-Object { $_ -and $_ -ne $Destination -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)
    if (!$Sources.Count) { return }
    # Preserve user-created files. Factory catalog/font resources always come
    # from the new package. Old directories and databases stay available.
    $files = @($script:IpaPersonalFiles)
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
            if ($KeepNewerDestination) {
                $current = Get-Item -LiteralPath $to -ErrorAction SilentlyContinue
                if ($current -and $current.LastWriteTimeUtc -ge $candidates[0].LastWriteTimeUtc) { continue }
            }
            New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
            if (Test-Path -LiteralPath $to) { Copy-Item -LiteralPath $to -Destination ($to + '.before-upgrade-' + (Get-Date -Format 'yyyyMMddHHmmssfff')) }
            Copy-Item -LiteralPath $from -Destination $to -Force
        }
    }
}
# Data folders of earlier releases, which ran from the extraction folder. Program Files copies
# hold only factory resources and are never migration sources.
function Get-IpaLegacyDataSources {
    $sources = @()
    $previous = Get-IpaInstallation
    if ($previous -and $previous.DataDir) { $sources += [string]$previous.DataDir }
    $sources += @(Get-IpaProcesses | Where-Object { $_.ExecutablePath } | ForEach-Object { Join-Path (Split-Path -Parent $_.ExecutablePath) 'data' })
    @($sources | Where-Object { $_ -and !(Test-IpaUnderProgramRoot $_) } | Select-Object -Unique)
}
function Add-IpaNativeMethods {
    if (!('PhoneticIpa.NativeMethods' -as [type])) {
        Add-Type -Namespace PhoneticIpa -Name NativeMethods -MemberDefinition '[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern bool MoveFileEx(string existing, string replacement, int flags);'
    }
}
# Removes a program folder under Program Files. Files still loaded by running applications
# (the TSF DLL inside open programs) are deleted at the next restart instead.
function Remove-IpaPath([string]$Path) {
    if (!(Test-IpaUnderProgramRoot $Path) -or !(Test-Path -LiteralPath $Path)) { return }
    # The marker goes last, so a folder that cannot be emptied stays recognizable.
    $marker = Join-Path $Path $script:IpaAppMarker
    Get-ChildItem -LiteralPath $Path -Force | Where-Object { $_.FullName -ne $marker } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    if (@(Get-ChildItem -LiteralPath $Path -Force | Where-Object { $_.FullName -ne $marker }).Count -eq 0) {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction SilentlyContinue
        return
    }
    Add-IpaNativeMethods
    $items = @(Get-ChildItem -LiteralPath $Path -Recurse -Force | Sort-Object { $_.FullName.Length } -Descending) + @(Get-Item -LiteralPath $Path -Force)
    # [NullString]::Value: PowerShell would pass $null as an empty string, which MoveFileEx rejects.
    foreach ($item in $items) { [void][PhoneticIpa.NativeMethods]::MoveFileEx($item.FullName, [NullString]::Value, 4) } # MOVEFILE_DELAY_UNTIL_REBOOT
}
# Deletes earlier program folders this installer created (marked app-* folders). Personal data
# and extraction folders are never touched.
function Remove-IpaOldPrograms([string]$Keep) {
    if (!(Test-Path -LiteralPath $script:IpaProgramRoot)) { return }
    foreach ($dir in @(Get-ChildItem -LiteralPath $script:IpaProgramRoot -Directory | Where-Object { $_.Name -like 'app-*' })) {
        if ($Keep -and [IO.Path]::GetFullPath($dir.FullName).TrimEnd('\') -ieq [IO.Path]::GetFullPath($Keep).TrimEnd('\')) { continue }
        # Marked folders, or folders named exactly as this installer names them (a marker may
        # already be gone when an earlier removal was interrupted).
        if ((Test-Path -LiteralPath (Join-Path $dir.FullName $script:IpaAppMarker)) -or $dir.Name -match '^app-\d+\.\d+\.\d+-\d{14}$') { Remove-IpaPath $dir.FullName }
    }
}
# Copies the verified runtime to a new Program Files folder. A fresh folder per install avoids
# replacing DLLs that running applications still have loaded.
function Copy-IpaProgram([string]$Source, [string]$ScriptDirectory, [switch]$SourceWasLive) {
    $target = Join-Path $script:IpaProgramRoot ('app-' + $script:IpaVersion + '-' + (Get-Date -Format 'yyyyMMddHHmmss'))
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    $sourceRoot = [IO.Path]::GetFullPath($Source).TrimEnd('\')
    foreach ($item in @(Get-ChildItem -LiteralPath $Source -Recurse -File -Force)) {
        $relative = $item.FullName.Substring($sourceRoot.Length + 1)
        if ($relative -like 'data\*') {
            $dataRelative = $relative.Substring(5)
            # Factory resources only. The package's config.toml is the factory default, unless an
            # older release used this folder as its live data folder.
            if ((Test-IpaPersonalFile $dataRelative) -and ($SourceWasLive -or $dataRelative -ne 'config.toml')) { continue }
        }
        $to = Join-Path $target $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
        Copy-Item -LiteralPath $item.FullName -Destination $to -Force
    }
    New-Item -ItemType Directory -Path (Join-Path $target 'data') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $target 'data\factory-stamp.txt') -Value (Split-Path -Leaf $target) -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $target $script:IpaAppMarker) -Value $script:IpaVersion -Encoding ASCII
    $scripts = Join-Path $script:IpaProgramRoot 'scripts'
    New-Item -ItemType Directory -Path $scripts -Force | Out-Null
    foreach ($name in @('Windows-Installation.ps1', 'Uninstall-Windows.ps1')) {
        Copy-Item -LiteralPath (Join-Path $ScriptDirectory $name) -Destination (Join-Path $scripts $name) -Force
    }
    Assert-IpaRuntime $target
    $target
}
function Install-IpaRuntime([string]$Runtime, [string]$ScriptDirectory, [string]$UserDataDir = '') {
    Assert-IpaRuntime $Runtime
    $previous = Get-IpaInstallation
    $oldRuntime = if ($previous -and $previous.ServerPath) { Split-Path -Parent $previous.ServerPath } else { $null }
    if ($oldRuntime -and !(Test-IpaRuntime $oldRuntime)) { $oldRuntime = $null }
    $legacy = @(Get-IpaLegacyDataSources)
    $sourceWasLive = [bool]($previous -and $previous.DataDir -and [IO.Path]::GetFullPath([string]$previous.DataDir).TrimEnd('\') -ieq [IO.Path]::GetFullPath((Join-Path $Runtime 'data')).TrimEnd('\'))
    Stop-IpaProcesses
    # Copy, never move: the old extraction folder keeps its files.
    if ($UserDataDir) { Copy-IpaPreferences $legacy $UserDataDir -KeepNewerDestination }
    $target = Copy-IpaProgram $Runtime $ScriptDirectory -SourceWasLive:$sourceWasLive
    try {
        # The new DLL has the same independent CLSID, so it can also remove a
        # stale registration whose old extraction directory has disappeared.
        Invoke-IpaRegistration $target -Unregister
        Set-IpaInstallation $target
        Invoke-IpaRegistration $target
        New-Item -Path $script:IpaUninstallRegistry -Force | Out-Null
        $powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $uninstaller = Join-Path $script:IpaProgramRoot 'scripts\Uninstall-Windows.ps1'
        $command = '"' + $powershell + '" -NoProfile -ExecutionPolicy Bypass -File "' + $uninstaller + '"'
        foreach ($pair in @(@('DisplayName','国际音标输入法'), @('DisplayVersion',$script:IpaVersion), @('Publisher','Nephelium'), @('InstallLocation',$target), @('UninstallString',$command), @('DisplayIcon',(Join-Path $target 'PhoneticIpaSettings.exe')))) {
            New-ItemProperty -LiteralPath $script:IpaUninstallRegistry -Name $pair[0] -Value $pair[1] -PropertyType String -Force | Out-Null
        }
        foreach ($name in @('NoModify','NoRepair')) { New-ItemProperty -LiteralPath $script:IpaUninstallRegistry -Name $name -Value 1 -PropertyType DWord -Force | Out-Null }
    } catch {
        $failure = $_
        if ($oldRuntime) {
            New-Item -Path $script:IpaRegistry -Force | Out-Null
            New-ItemProperty -LiteralPath $script:IpaRegistry -Name ServerPath -Value (Join-Path $oldRuntime 'PhoneticIpaServer.exe') -PropertyType String -Force | Out-Null
            if ($previous.DataDir) { New-ItemProperty -LiteralPath $script:IpaRegistry -Name DataDir -Value $previous.DataDir -PropertyType String -Force | Out-Null }
            if ($previous.FactoryDataDir) { New-ItemProperty -LiteralPath $script:IpaRegistry -Name FactoryDataDir -Value $previous.FactoryDataDir -PropertyType String -Force | Out-Null }
            Invoke-IpaRegistration $oldRuntime
        } else {
            Invoke-IpaRegistration $target -Unregister
            Remove-Item -LiteralPath $script:IpaRegistry -Recurse -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath $script:IpaUninstallRegistry -Recurse -ErrorAction SilentlyContinue
        }
        Remove-IpaPath $target
        throw $failure
    }
    Remove-IpaOldPrograms $target
    $target
}
function Uninstall-IpaRuntime([string]$FallbackRuntime = '') {
    $installed = Get-IpaInstallation
    $runtime = if ($installed -and $installed.ServerPath) { Split-Path -Parent $installed.ServerPath } else { $FallbackRuntime }
    Stop-IpaProcesses -SkipValidation
    if ($runtime -and (Test-IpaRuntime $runtime)) {
        Invoke-IpaRegistration $runtime -Unregister
        Remove-IpaTipRegistration # The DLL leaves empty TIP category keys behind.
    }
    else { Remove-IpaTipRegistration } # Missing or changed files: never run their code.
    foreach ($path in @("Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\$script:IpaClsid", "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\WOW6432Node\CLSID\$script:IpaClsid")) {
        if (Test-Path -LiteralPath $path) { throw '卸载注册未完成，已保留卸载入口，请重试。' }
    }
    Remove-Item -LiteralPath $script:IpaRegistry -Recurse -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $script:IpaUninstallRegistry -Recurse -ErrorAction SilentlyContinue
    Remove-IpaOldPrograms ''
    Remove-IpaPath (Join-Path $script:IpaProgramRoot 'scripts')
    Write-Host '国际音标输入法已卸载。程序文件已删除（正被其他程序使用的文件在重启后删除），个人键位、配置和解压文件均已保留。'
}
