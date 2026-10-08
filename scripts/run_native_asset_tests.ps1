[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'src_rebuild'
$output = Join-Path $sourceRoot 'build\native-renderer-r02\asset-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'MSVC x64 tools were not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$suite = Join-Path $sourceRoot 'tests\NativeAssetTests.cpp'
$implementation = Join-Path $sourceRoot 'utils\NativeAssetDecode.cpp'
$texture = Join-Path $sourceRoot 'utils\NativeTextureDecode.cpp'
$cache = Join-Path $sourceRoot 'utils\NativeAssetCache.cpp'
$catalog = Join-Path $sourceRoot 'Game\C\assetcatalog.c'
$modern = Join-Path $sourceRoot 'utils\NativeModernDecode.cpp'
$gltf = Join-Path $sourceRoot 'utils\GltfLoader.cpp'
$gpuCache = Join-Path $sourceRoot 'utils\NativeGpuAssetCache.cpp'
$nativeOwner = Join-Path $sourceRoot 'PsyCross\src\render\PsyX_NativeScene.cpp'
$mip = Join-Path $sourceRoot 'PsyCross\src\render\PsyX_NativeMip.cpp'
$backdrop = Join-Path $sourceRoot 'utils\NativeBackdropDecode.cpp'
$fixture = Join-Path $repoRoot 'assets\modern_fixtures\bollard.glb'
$fixtureHash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
$include = Join-Path $sourceRoot 'PsyCross\include'
$exe = Join-Path $output 'NativeAssetTests.exe'
foreach ($path in @($vcvars,$suite,$implementation,$texture,$cache,$catalog,$modern,$gltf,$gpuCache,$nativeOwner,$mip,$backdrop,$fixture,$include,$exe))
{
    if ($path -match '["%\r\n&|<>^!]') { throw "Unsupported batch path: $path" }
}
$command = 'call "{0}" >nul && cl /nologo /EHsc /W4 /O2 /TP /I"{1}" "{2}" "{3}" "{5}" "{6}" "{7}" "{8}" "{9}" "{10}" "{11}" "{12}" "{13}" /Fe:"{4}" /link ole32.lib windowscodecs.lib' -f $vcvars,$include,$suite,$implementation,$exe,$texture,$cache,$catalog,$modern,$gltf,$gpuCache,$nativeOwner,$backdrop,$mip
Push-Location $output
try
{
    & $env:ComSpec /d /s /c $command > build.txt 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Native asset tests failed to build; see $output\build.txt" }
    & $exe $fixture | Tee-Object -FilePath results.txt
    if ($LASTEXITCODE -ne 0) { throw 'Original asset decode integrity failed.' }
    if ((Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash -ne $fixtureHash)
    {
        throw 'Owned GLB fixture changed during read-only validation.'
    }
}
finally { Pop-Location }
