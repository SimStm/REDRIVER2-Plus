[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'src_rebuild'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'Visual Studio C++ tools were not found.' }
$vcvars = Join-Path $installation.Trim() 'VC\Auxiliary\Build\vcvars64.bat'
# Executables are built at a stable path so Windows remembers the firewall choice
# for them; each run still gets a fresh working directory because some fixtures
# are non-idempotent.
$binRoot = Join-Path $sourceRoot 'build\presentation-timing-tests\bin'
New-Item -ItemType Directory -Path $binRoot -Force | Out-Null
$outputRoot = Join-Path $sourceRoot ('build\presentation-timing-tests\run\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

# These tests substitute only the SDL counter and the stateful camera controller.
# Production clock/snapshot sources are linked directly; no game assets or GPU.
foreach ($region in @('NTSC', 'PAL'))
{
	$outputDirectory = Join-Path $outputRoot $region
	New-Item -ItemType Directory -Path $outputDirectory | Out-Null
	$executable = Join-Path $binRoot 'PresentationTimingTests.exe'
	$arguments = @('/nologo', '/TP', '/EHsc', '/std:c++14', '/O2', '/DSDL_MAIN_HANDLED', "/D${region}_VERSION",
		'/I Game', '/I PsyCross\include', '/I PsyCross\include\psx', '/I dependencies\SDL2-2.30.2\include',
		'tests\PresentationTimingTests.cpp', 'utils\ProjectClock.cpp', 'utils\VisualSnapshot.cpp',
		('/Fo"' + $outputDirectory + '\\"'), ('/Fe:"' + $executable + '"'))
	Push-Location $sourceRoot
	try
	{
		$compileOutput = & cmd.exe /c ('call "' + $vcvars + '" >nul 2>&1 && cl.exe ' + ($arguments -join ' ') + ' 2>&1')
		$compileCode = $LASTEXITCODE
	}
	finally { Pop-Location }
	$compileOutput | Set-Content -LiteralPath (Join-Path $outputDirectory 'build.log')
	if ($compileCode -ne 0)
	{
		$compileOutput | ForEach-Object { Write-Host $_ }
		throw "$region timing tests failed to build (exit $compileCode)."
	}
	$testOutput = & $executable 2>&1
	$testCode = $LASTEXITCODE
	$testOutput | Tee-Object -FilePath (Join-Path $outputDirectory 'results.log') | Write-Host
	if ($testCode -ne 0) { throw "$region timing tests failed (exit $testCode)." }

	$settingsExecutable = Join-Path $binRoot 'FrameSettingsTests.exe'
	$settingsArguments = @('/nologo', '/TP', '/EHsc', '/std:c++14', '/O2', '/DUNICODE', '/D_UNICODE', '/DSDL_MAIN_HANDLED', "/D${region}_VERSION",
		'/I Game', '/I PsyCross\include', '/I PsyCross\include\psx', '/I dependencies\SDL2-2.30.2\include',
		'tests\FrameSettingsTests.cpp', 'utils\ProjectClock.cpp', 'utils\DeveloperGraphicsSettings.cpp', 'utils\DeveloperSettingsFile.cpp',
		('/Fo"' + $outputDirectory + '\\"'), ('/Fe:"' + $settingsExecutable + '"'))
	Push-Location $sourceRoot
	try
	{
		$settingsBuild = & cmd.exe /c ('call "' + $vcvars + '" >nul 2>&1 && cl.exe ' + ($settingsArguments -join ' ') + ' 2>&1')
		$settingsBuildCode = $LASTEXITCODE
	}
	finally { Pop-Location }
	$settingsBuild | Set-Content -LiteralPath (Join-Path $outputDirectory 'settings-build.log')
	if ($settingsBuildCode -ne 0)
	{
		$settingsBuild | ForEach-Object { Write-Host $_ }
		throw "$region frame settings tests failed to build (exit $settingsBuildCode)."
	}
	# Persistence fixtures are non-idempotent and must never touch the user's files.
	Push-Location $outputDirectory
	try
	{
		$settingsOutput = & $settingsExecutable 2>&1
		$settingsCode = $LASTEXITCODE
	}
	finally { Pop-Location }
	$settingsOutput | Tee-Object -FilePath (Join-Path $outputDirectory 'settings-results.log') | Write-Host
	if ($settingsCode -ne 0) { throw "$region frame settings tests failed (exit $settingsCode)." }

	$effectsExecutable = Join-Path $binRoot 'EffectTimingTests.exe'
	$effectsArguments = @('/nologo', '/TP', '/EHsc', '/std:c++14', '/O2', '/Gy', '/DSDL_MAIN_HANDLED', "/D${region}_VERSION",
		'/I Game', '/I Game\C', '/I PsyCross\include', '/I PsyCross\include\psx', '/I dependencies\SDL2-2.30.2\include',
		'tests\EffectTimingTests.cpp', 'utils\ProjectClock.cpp',
		('/Fo"' + $outputDirectory + '\\"'), ('/Fe:"' + $effectsExecutable + '"'), '/link', '/OPT:REF')
	Push-Location $sourceRoot
	try
	{
		$effectsBuild = & cmd.exe /c ('call "' + $vcvars + '" >nul 2>&1 && cl.exe ' + ($effectsArguments -join ' ') + ' 2>&1')
		$effectsBuildCode = $LASTEXITCODE
	}
	finally { Pop-Location }
	$effectsBuild | Set-Content -LiteralPath (Join-Path $outputDirectory 'effects-build.log')
	if ($effectsBuildCode -ne 0)
	{
		$effectsBuild | ForEach-Object { Write-Host $_ }
		throw "$region effect timing tests failed to build (exit $effectsBuildCode)."
	}
	$effectsOutput = & $effectsExecutable 2>&1
	$effectsCode = $LASTEXITCODE
	$effectsOutput | Tee-Object -FilePath (Join-Path $outputDirectory 'effects-results.log') | Write-Host
	if ($effectsCode -ne 0) { throw "$region effect timing tests failed (exit $effectsCode)." }
}
Write-Host "Presentation timing tests passed. Evidence: $outputRoot"
