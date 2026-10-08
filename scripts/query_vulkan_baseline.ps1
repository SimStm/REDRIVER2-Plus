[CmdletBinding()]
param([string]$OutDir = '', [string]$VisualStudio = '')

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (!$OutDir) { $OutDir = Join-Path $repoRoot 'src_rebuild\build\native-renderer-r00' }
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
if (!$VisualStudio)
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or !$VisualStudio) { throw 'MSVC x64 tools were not found.' }
}
$vcvars = Join-Path $VisualStudio 'VC\Auxiliary\Build\vcvars64.bat'
$headers = Join-Path $repoRoot 'src_rebuild\dependencies\vulkan\include'
if ($env:VULKAN_DIR) { $headers = Join-Path $env:VULKAN_DIR 'include' }
if (!(Test-Path -LiteralPath (Join-Path $headers 'vulkan\vulkan.h'))) { throw 'Vulkan headers were not found.' }
$source = Join-Path $PSScriptRoot 'query_vulkan_baseline.cpp'
$exe = Join-Path $OutDir 'query_vulkan_baseline.exe'
$obj = Join-Path $OutDir 'query_vulkan_baseline.obj'
# cmd quoting is required by vcvars; paths with expansion/control characters
# are rejected rather than interpolated into batch source.
foreach ($path in @($vcvars, $headers, $source, $exe, $obj))
{
    if ($path -match '["%\r\n&|<>^!]') { throw "Unsupported batch path: $path" }
}
$buildCommand = 'call "{0}" >nul && cl /nologo /EHsc /W4 /O2 /I"{1}" "{2}" /Fe:"{3}" /Fo:"{4}"' -f $vcvars, $headers, $source, $exe, $obj
& $env:ComSpec /d /s /c $buildCommand > (Join-Path $OutDir 'capabilities-build.txt') 2>&1
if ($LASTEXITCODE -ne 0) { throw "Capability probe build failed; see $OutDir\capabilities-build.txt" }
& $exe > (Join-Path $OutDir 'capabilities.json') 2> (Join-Path $OutDir 'capabilities-loader.txt')
if ($LASTEXITCODE -ne 0) { throw "Capability probe failed; see $OutDir\capabilities-loader.txt" }
$report = Get-Content -LiteralPath (Join-Path $OutDir 'capabilities.json') -Raw | ConvertFrom-Json
if ($report.schema -ne 1 -or !$report.devices) { throw 'Capability probe returned an invalid report.' }
$report.devices | ForEach-Object {
    Write-Output ('{0}: colour/depth samples {1}; memory budget query {2}' -f $_.name, ($_.colorDepthSamples -join ', '), $_.memoryBudgetSupported)
    Write-Output ('Sampling precision: texel {0}, mip {1}, raster {2} fractional bits' -f
        $_.samplingPrecision.subTexelBits, $_.samplingPrecision.mipmapBits, $_.samplingPrecision.subPixelBits)
}
Write-Output "Report: $OutDir\capabilities.json"
