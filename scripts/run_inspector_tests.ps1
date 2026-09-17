[CmdletBinding()]
param(
	# Keep the working directories instead of deleting them, for inspection.
	[switch] $KeepWorkingDirectories
)

$ErrorActionPreference = 'Stop'

# Builds and runs the two standalone suites that cover the asset catalog and the
# inspector export path. They need no game assets and no GL context, so they are
# the fastest verification available for changes in those areas.
#
# InspectorExportTests is not idempotent: it mutates the working directory and
# reports leftover state as failures when re-run in the same place. Every run
# therefore gets a fresh directory.
#
# Both suites link Windows-only code (WIC), so this script is Windows-only. The
# game itself still builds for the other targets.

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'src_rebuild'
$buildDirectory = Join-Path $sourceRoot 'build'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'dependencies\SDL2-2.30.2\include')))
{
	throw 'SDL2 headers not found. Run windows_dev_prepare.ps1 first.'
}

$vcvarsCandidates = @(
	'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
)

$vcvars = $null
foreach ($candidate in $vcvarsCandidates)
{
	if (Test-Path -LiteralPath $candidate) { $vcvars = $candidate; break }
}

if (-not $vcvars -and (Test-Path -LiteralPath $vswhere))
{
	$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
		Select-Object -First 1
	if ($installation)
	{
		$candidate = Join-Path $installation.Trim() 'VC\Auxiliary\Build\vcvars64.bat'
		if (Test-Path -LiteralPath $candidate) { $vcvars = $candidate }
	}
}

if (-not $vcvars)
{
	throw 'A Visual Studio C++ toolset (vcvars64.bat) was not found.'
}

$workingRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("redriver2-inspector-tests-" + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $workingRoot | Out-Null

function Invoke-Suite
{
	param(
		[string] $Name,
		[string[]] $Sources,
		[string[]] $ExtraIncludes,
		[string[]] $LinkLibraries
	)

	$executable = Join-Path $buildDirectory "$Name.exe"
	$arguments = @('/nologo', '/TP', '/EHsc', '/std:c++14', '/O2', '/Gy', '/I', 'PsyCross\include')
	foreach ($include in $ExtraIncludes) { $arguments += @('/I', $include) }
	$arguments += $Sources
	$arguments += @("/Fe:$executable")
	$arguments += @('/link', '/OPT:REF')
	foreach ($library in $LinkLibraries) { $arguments += $library }

	Push-Location $sourceRoot
	try
	{
		$compileOutput = & cmd.exe /c ('call "' + $vcvars + '" >nul 2>&1 && cl.exe ' + ($arguments -join ' ') + ' 2>&1')
		$compileCode = $LASTEXITCODE
	}
	finally
	{
		Pop-Location
	}

	if ($compileCode -ne 0)
	{
		$compileOutput | ForEach-Object { Write-Host $_ }
		throw "$Name failed to compile (exit $compileCode)."
	}

	$runDirectory = Join-Path $workingRoot $Name
	New-Item -ItemType Directory -Path $runDirectory | Out-Null

	Push-Location $runDirectory
	try
	{
		$output = & $executable 2>&1
		$code = $LASTEXITCODE
	}
	finally
	{
		Pop-Location
	}

	$failures = @($output | Select-String -Pattern '^FAIL')
	foreach ($failure in $failures) { Write-Host $failure.Line }

	if ($code -ne 0 -or $failures.Count -gt 0)
	{
		throw "$Name reported $($failures.Count) failure(s) (exit $code). Output kept in $runDirectory."
	}

	# InspectorExportTests prints one PASS line per check; AssetCatalogTests prints
	# a single summary line. Report whichever the suite provides.
	$passes = @($output | Select-String -Pattern '^PASS').Count
	if ($passes -gt 0)
	{
		Write-Host "$Name`: $passes checks passed"
	}
	else
	{
		Write-Host "$Name`: $(@($output | Where-Object { $_ -match '\S' } | Select-Object -Last 1))"
	}
}

try
{
	Invoke-Suite -Name 'AssetCatalogTests' -Sources @('tests\AssetCatalogTests.cpp', 'Game\C\assetcatalog.c') `
		-ExtraIncludes @() -LinkLibraries @()
	Invoke-Suite -Name 'InspectorExportTests' -Sources @('tests\InspectorExportTests.cpp') `
		-ExtraIncludes @('dependencies\SDL2-2.30.2\include') -LinkLibraries @('windowscodecs.lib', 'ole32.lib')

	Write-Host 'All inspector suites passed.'
}
finally
{
	if ($KeepWorkingDirectories)
	{
		Write-Host "Working directories kept in $workingRoot"
	}
	else
	{
		Remove-Item -LiteralPath $workingRoot -Recurse -Force -ErrorAction SilentlyContinue
	}
}
