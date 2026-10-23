param(
    [ValidateSet('server','x64','x86','ui')][string]$Component = 'server',
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$CMake = 'cmake',
    [string]$BoostRoot = '',
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$repoRoot = Split-Path $taskRoot -Parent
if (!$VcpkgRoot) { $VcpkgRoot = Join-Path $taskRoot 'tools\vcpkg' }
$source = if ($Component -eq 'ui') { 'ui' } elseif ($Component -eq 'server') { 'server' } else { 'windows' }
$arch = if ($Component -eq 'x86') { 'Win32' } else { 'x64' }
$triplet = if ($Component -eq 'server') { 'x64-windows' } elseif ($Component -eq 'x86') { 'x86-windows-static' } else { 'x64-windows-static' }
$build = Join-Path $taskRoot "output\build-$Component"
$env:METASEQUOIA_IME_DATA_DIR = Join-Path $taskRoot 'output\runtime\data'
$env:METASEQUOIA_IME_CONFIG_DIR = $env:METASEQUOIA_IME_DATA_DIR
$arguments = @('-S', (Join-Path $repoRoot $source), '-B', $build, '-G', 'Visual Studio 17 2022', '-A', $arch, '-DBUILD_TESTING=ON')
if ($Component -ne 'ui') {
    $toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
    if (!(Test-Path -LiteralPath $toolchain)) { throw 'Provide an existing vcpkg checkout with -VcpkgRoot.' }
    $arguments += "-DCMAKE_TOOLCHAIN_FILE=$toolchain", "-DVCPKG_TARGET_TRIPLET=$triplet"
}
if ($Component -eq 'server') {
    if (!$BoostRoot) { $BoostRoot = Join-Path $VcpkgRoot 'installed\x64-windows-static-md' }
    $arguments += "-DBoost_ROOT=$BoostRoot"
}
& $CMake @arguments
if ($LASTEXITCODE -ne 0) { throw "Configure failed: $Component" }
if (!$ConfigureOnly) {
    & $CMake --build $build --config Release --parallel 8
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Component" }
}
