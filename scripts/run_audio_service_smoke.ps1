[CmdletBinding()]
param()

# E04: builds and runs the FMOD Core audio service smoke test against the SDK
# acquired by scripts/fetch_fmod.ps1. Windows-only; it needs a working audio
# output device (the smoke prints a distinct message and exits non-zero when no
# device is available) and no game assets.

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'src_rebuild'
$fmodBase = if ($env:FMOD_DIR) { $env:FMOD_DIR } else { Join-Path $sourceRoot 'dependencies\fmod' }
$fmodRoot = Join-Path $fmodBase 'core'
if (-not (Test-Path (Join-Path $fmodRoot 'inc\fmod.h'))) { $fmodRoot = Join-Path $fmodBase 'api\core' }
$fmodInclude = Join-Path $fmodRoot 'inc'
$fmodLibrary = Join-Path $fmodRoot 'lib\x64\fmod_vc.lib'
$fmodRuntime = Join-Path $fmodRoot 'lib\x64\fmod.dll'

if (-not (Test-Path -LiteralPath $fmodLibrary) -or -not (Test-Path -LiteralPath $fmodRuntime))
{
	throw 'The FMOD Core SDK was not found under src_rebuild/dependencies/fmod/core. Run scripts/fetch_fmod.ps1 first.'
}

$vcvarsCandidates = @(
	'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat'
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vcvars)
{
	$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (Test-Path -LiteralPath $vswhere)
	{
		$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
			Select-Object -First 1
		if ($installation)
		{
			$candidate = Join-Path $installation.Trim() 'VC\Auxiliary\Build\vcvars64.bat'
			if (Test-Path -LiteralPath $candidate) { $vcvars = $candidate }
		}
	}
}
if (-not $vcvars)
{
	throw 'A Visual Studio C++ toolset (vcvars64.bat) was not found.'
}

# A stable output path so the copied fmod.dll and the run stay in one known
# directory across invocations.
$workingRoot = Join-Path $sourceRoot 'build\audio-service-smoke'
New-Item -ItemType Directory -Path $workingRoot -Force | Out-Null
$executable = Join-Path $workingRoot 'AudioServiceSmoke.exe'

$arguments = @('/nologo', '/std:c++17', '/EHsc', '/O2', '/Gy', '/MD', '/TP', '/I', ('"' + $fmodInclude + '"'))
$arguments += @('tests\AudioServiceSmoke.cpp', 'utils\AudioService.cpp', 'Game\C\audiocontract.c')
$arguments += @(('/Fo:"' + $workingRoot + '\\"'), ('/Fe:"' + $executable + '"'))
$arguments += @('/link', '/OPT:REF')
$arguments += @('"' + $fmodLibrary + '"')

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
	throw "AudioServiceSmoke failed to compile/link (exit $compileCode)."
}

Copy-Item -LiteralPath $fmodRuntime -Destination (Join-Path $workingRoot 'fmod.dll') -Force
$headerMatch = Select-String -LiteralPath (Join-Path $fmodInclude 'fmod_common.h') -Pattern '#define\s+FMOD_VERSION\s+(0x[0-9A-Fa-f]+)' | Select-Object -First 1
$packed = [Convert]::ToUInt32($headerMatch.Matches[0].Groups[1].Value.Substring(2), 16)
$expectedVersion = '{0:X}.{1:X2}.{2:X2}' -f ($packed -shr 16), (($packed -shr 8) -band 255), ($packed -band 255)
$dll = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($fmodRuntime)
$dllMatch = [regex]::Match($dll.ProductVersion, '^(\d+)\.(\d+)\.(\d+)$')
if (-not $dllMatch.Success) { throw "Unrecognized FMOD DLL ProductVersion: $($dll.ProductVersion)" }
$dllVersion = '{0}.{1:D2}.{2:D2}' -f [int]$dllMatch.Groups[1].Value, [int]$dllMatch.Groups[2].Value, [int]$dllMatch.Groups[3].Value
if ($dllVersion -ne $expectedVersion) { throw "FMOD header $expectedVersion does not match DLL $dllVersion" }
Write-Host "FMOD header/DLL metadata agree: $expectedVersion"

Push-Location $workingRoot
try
{
	$output = & $executable $expectedVersion 2>&1
	$code = $LASTEXITCODE
}
finally
{
	Pop-Location
}

$output | ForEach-Object { Write-Host $_ }
if ($code -ne 0)
{
	throw "AudioServiceSmoke reported failures (exit $code). Output kept in $workingRoot."
}

Write-Host "Audio service smoke passed. Output kept in $workingRoot"
