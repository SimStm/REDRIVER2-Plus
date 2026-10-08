[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'src_rebuild'
$output = Join-Path $sourceRoot 'build\native-renderer-r01\scene-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'MSVC x64 tools were not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$suite = Join-Path $sourceRoot 'tests\NativeSceneTests.cpp'
$implementation = Join-Path $sourceRoot 'PsyCross\src\render\PsyX_NativeScene.cpp'
$mip = Join-Path $sourceRoot 'PsyCross\src\render\PsyX_NativeMip.cpp'
$camera = Join-Path $sourceRoot 'utils\DeveloperNativeCamera.cpp'
$backend = Join-Path $sourceRoot 'PsyCross\src\render\PsyX_Vk.cpp'
$adapter = Join-Path $sourceRoot 'utils\DeveloperNativeScene.cpp'
$assetsAdapter = Join-Path $sourceRoot 'utils\DeveloperNativeAssets.cpp'
$regionAdapter = Join-Path $sourceRoot 'utils\DeveloperNativeRegion.cpp'
$backdropAdapter = Join-Path $sourceRoot 'utils\DeveloperNativeBackdrop.cpp'
$cProbe = Join-Path $sourceRoot 'tests\NativeSceneCProbe.c'
$include = Join-Path $sourceRoot 'PsyCross\include'
$sdlRoot = if ($env:SDL2_DIR) { $env:SDL2_DIR } else { Join-Path $sourceRoot 'dependencies\SDL2-2.30.2' }
$sdlInclude = Join-Path $sdlRoot 'include'
$exe = Join-Path $output 'NativeSceneTests.exe'
foreach ($path in @($vcvars, $suite, $implementation, $mip, $camera, $backend, $adapter, $assetsAdapter, $regionAdapter, $backdropAdapter, $cProbe, $include, $sdlInclude, $exe))
{
    if ($path -match '["%\r\n&|<>^!]') { throw "Unsupported batch path: $path" }
}
$command = 'call "{0}" >nul && cl /nologo /W4 /O2 /I"{5}" /TC /c "{4}" /Fo:NativeSceneCProbe.obj && cl /nologo /EHsc /W4 /O2 /I"{5}" "{1}" "{2}" "{6}" "{7}" NativeSceneCProbe.obj /Fe:"{3}"' -f $vcvars, $suite, $implementation, $exe, $cProbe, $include, $camera, $mip
# Host compilation verifies the excluded Vulkan/adapter branches, not a complete
# Android, Emscripten or PlayStation target build/toolchain.
foreach ($guard in @('__ANDROID__', '__EMSCRIPTEN__', 'PSX'))
{
    $command += ' && cl /nologo /EHsc /W4 /c /D{0} /I"{1}" /I"{4}" "{2}" /Fo:VkGuard{0}.obj && cl /nologo /EHsc /W4 /c /D{0} /I"{1}" "{3}" /Fo:AdapterGuard{0}.obj' -f $guard, $include, $backend, $adapter, $sdlInclude
    $command += ' && cl /nologo /EHsc /W4 /c /D{0} /I"{1}" "{2}" /Fo:AssetsAdapterGuard{0}.obj' -f $guard, $include, $assetsAdapter
    $command += ' && cl /nologo /EHsc /W4 /c /D{0} /I"{1}" "{2}" /Fo:RegionAdapterGuard{0}.obj' -f $guard, $include, $regionAdapter
    $command += ' && cl /nologo /EHsc /W4 /c /D{0} /I"{1}" "{2}" /Fo:BackdropAdapterGuard{0}.obj' -f $guard, $include, $backdropAdapter
}
Push-Location $output
try
{
    & $env:ComSpec /d /s /c $command > build.txt 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Native scene tests failed to build; see $output\build.txt" }
    & $exe | Tee-Object -FilePath results.txt
    if ($LASTEXITCODE -ne 0) { throw 'Native scene ownership invariants failed.' }
}
finally { Pop-Location }
