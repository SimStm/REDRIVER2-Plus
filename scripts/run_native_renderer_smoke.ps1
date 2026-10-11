[CmdletBinding()]
param()

# E03: builds and runs the native renderer smoke test against the Diligent
# libraries produced by scripts/fetch_diligent.ps1. Windows-only; it needs a
# Vulkan-capable GPU/driver and no game assets.

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'src_rebuild'
$diligentRoot = Join-Path $sourceRoot 'dependencies\DiligentEngine'
$coreRoot = Join-Path $diligentRoot 'DiligentCore'
$buildRoot = Join-Path $diligentRoot 'build_core'
$engineLib = Join-Path $buildRoot 'Graphics\GraphicsEngineVulkan\Release\Diligent-GraphicsEngineVk-static.lib'

if (-not (Test-Path -LiteralPath $engineLib))
{
	throw 'Diligent is not built. Run scripts/fetch_diligent.ps1 first.'
}

$vcvarsCandidates = @(
	'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat',
	'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vcvars)
{
	throw 'A Visual Studio C++ toolset (vcvars64.bat) was not found.'
}

$includeDirs = @(
	(Join-Path $coreRoot 'Graphics\GraphicsEngine\interface'),
	(Join-Path $coreRoot 'Graphics\GraphicsEngineVulkan\interface'),
	(Join-Path $coreRoot 'Graphics\GraphicsTools\interface'),
	(Join-Path $coreRoot 'Graphics\GraphicsAccessories\interface'),
	(Join-Path $coreRoot 'Common\interface'),
	(Join-Path $coreRoot 'Primitives\interface'),
	(Join-Path $coreRoot 'Platforms\interface'),
	(Join-Path $coreRoot 'ThirdParty\Vulkan-Headers\include'),
	(Join-Path $coreRoot 'ThirdParty\volk')
)

$libs = @(
	(Join-Path $buildRoot 'Graphics\GraphicsEngineVulkan\Release\Diligent-GraphicsEngineVk-static.lib'),
	(Join-Path $buildRoot 'Graphics\GraphicsEngine\Release\Diligent-GraphicsEngine.lib'),
	(Join-Path $buildRoot 'Graphics\GraphicsEngineNextGenBase\Release\Diligent-GraphicsEngineNextGenBase.lib'),
	(Join-Path $buildRoot 'Graphics\GraphicsAccessories\Release\Diligent-GraphicsAccessories.lib'),
	(Join-Path $buildRoot 'Graphics\ShaderTools\Release\Diligent-ShaderTools.lib'),
	(Join-Path $buildRoot 'Graphics\HLSL2GLSLConverterLib\Release\Diligent-HLSL2GLSLConverterLib.lib'),
	(Join-Path $buildRoot 'Common\Release\Diligent-Common.lib'),
	(Join-Path $buildRoot 'Primitives\Release\Diligent-Primitives.lib'),
	(Join-Path $buildRoot 'Platforms\Basic\Release\Diligent-BasicPlatform.lib'),
	(Join-Path $buildRoot 'Platforms\Win32\Release\Diligent-Win32Platform.lib'),
	(Join-Path $buildRoot 'ThirdParty\glslang\glslang\Release\glslang.lib'),
	(Join-Path $buildRoot 'ThirdParty\glslang\SPIRV\Release\SPIRV.lib'),
	(Join-Path $buildRoot 'ThirdParty\SPIRV-Tools\source\Release\SPIRV-Tools.lib'),
	(Join-Path $buildRoot 'ThirdParty\SPIRV-Tools\source\opt\Release\SPIRV-Tools-opt.lib'),
	(Join-Path $buildRoot 'ThirdParty\SPIRV-Cross\Release\spirv-cross-core.lib'),
	(Join-Path $buildRoot 'ThirdParty\volk\Release\volk.lib')
)

# A stable output path (not a fresh temp GUID) so Windows remembers the firewall
# choice for the executable and does not prompt on every run. The smoke itself
# uses no network; a prompt comes from the graphics driver's Vulkan layer.
$workingRoot = Join-Path $sourceRoot 'build\native-renderer-smoke'
New-Item -ItemType Directory -Path $workingRoot -Force | Out-Null
$executable = Join-Path $workingRoot 'NativeRendererSmoke.exe'

$arguments = @('/nologo', '/std:c++17', '/EHsc', '/O2', '/Gy', '/MD', '/DPLATFORM_WIN32=1', '/I', 'native')
$objectDirectory = Join-Path $workingRoot 'obj'
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
$arguments += ('/Fo"' + $objectDirectory + '/"')
foreach ($include in $includeDirs) { $arguments += @('/I', $include) }
$arguments += @('tests\NativeRendererSmoke.cpp', 'native\NativeRenderer.cpp', 'utils\GltfLoader.cpp', 'utils\GltfWriter.cpp', 'utils\NativeInstanceTransform.cpp', 'utils\NativeCameraView.cpp')
$arguments += @("/Fe:$executable")
$arguments += @('/link', '/OPT:REF')
$arguments += $libs
$arguments += @('user32.lib', 'gdi32.lib', 'comdlg32.lib', 'windowscodecs.lib', 'ole32.lib')

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
	throw "NativeRendererSmoke failed to compile/link (exit $compileCode)."
}

Push-Location $workingRoot
try
{
	$output = & $executable 2>&1
	$code = $LASTEXITCODE
}
finally
{
	Pop-Location
}

$output | ForEach-Object { Write-Host $_ }
if ($code -ne 0)
{
	throw "NativeRendererSmoke reported failures (exit $code). Output kept in $workingRoot."
}

Write-Host "Native renderer smoke passed. Output kept in $workingRoot"
