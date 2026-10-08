[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'src_rebuild'
$output = Join-Path $sourceRoot 'build\native-renderer-r00\coverage-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or !$installation) { throw 'MSVC x64 tools were not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$suite = Join-Path $sourceRoot 'tests\RenderCoverageTests.cpp'
$implementation = Join-Path $sourceRoot 'utils\DeveloperRenderCoverage.cpp'
$cProbe = Join-Path $sourceRoot 'tests\RenderCoverageCProbe.c'
$exe = Join-Path $output 'RenderCoverageTests.exe'
foreach ($path in @($vcvars, $suite, $implementation, $cProbe, $exe))
{
    if ($path -match '["%\r\n&|<>^!]') { throw "Unsupported batch path: $path" }
}
# Match the game's CRT policy while keeping both consumers at /W4.
$command = 'call "{0}" >nul && cl /nologo /W4 /O2 /TC /c "{4}" /Fo:RenderCoverageCProbe.obj && cl /nologo /EHsc /W4 /O2 /D_CRT_SECURE_NO_WARNINGS "{1}" "{2}" RenderCoverageCProbe.obj /Fe:"{3}"' -f $vcvars, $suite, $implementation, $exe, $cProbe
Push-Location $output
try
{
    & $env:ComSpec /d /s /c $command > build.txt 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Coverage tests failed to build; see $output\build.txt" }
    # Keep diagnostics disabled for this unit suite regardless of caller settings.
    $previousLogging = $env:PSYX_COVERAGE_LOG
    try
    {
        $env:PSYX_COVERAGE_LOG = '0'
        & $exe | Tee-Object -FilePath results.txt
        if ($LASTEXITCODE -ne 0) { throw 'Coverage counter invariants failed.' }
    }
    finally { $env:PSYX_COVERAGE_LOG = $previousLogging }
}
finally { Pop-Location }
