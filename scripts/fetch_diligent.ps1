[CmdletBinding()]
param(
	# Visual Studio generator; defaults to the one matching the project toolset.
	[string] $Generator = 'Visual Studio 18 2026',
	# Skip the CMake build and only pin/acquire the sources.
	[switch] $AcquireOnly
)

# E03 block 1: acquire and build the Diligent Engine SDK used by the native
# renderer.
#
# The SDK is an external dependency, so it lives in the gitignored
# `src_rebuild/dependencies/DiligentEngine` and is never committed. The checkout
# is a direct clone of the official repository (master by default, overridable
# with the DILIGENT_REF environment variable); an existing checkout is reused so
# a user-updated clone is built as-is. Only the Vulkan backend is built (the
# selected desktop scope), as a static Release library. Re-running is safe.

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'src_rebuild'
$engineRoot = Join-Path $sourceRoot 'dependencies\DiligentEngine'
$coreRoot = Join-Path $engineRoot 'DiligentCore'
$buildRoot = Join-Path $engineRoot 'build_core'

$umbrellaUrl = 'https://github.com/DiligentGraphics/DiligentEngine.git'
$umbrellaRef = if ($env:DILIGENT_REF) { $env:DILIGENT_REF } else { 'master' }
$requiredSubmodules = @(
	'ThirdParty/Vulkan-Headers',
	'ThirdParty/volk',
	'ThirdParty/xxHash',
	'ThirdParty/glslang',
	'ThirdParty/SPIRV-Cross',
	'ThirdParty/SPIRV-Tools',
	'ThirdParty/SPIRV-Headers'
)

if (-not (Test-Path -LiteralPath $engineRoot))
{
	Write-Host "Cloning DiligentEngine $umbrellaRef ..."
	git clone --depth 1 --branch $umbrellaRef $umbrellaUrl $engineRoot
}
else
{
	$engineCommit = git -C $engineRoot rev-parse --short HEAD
	Write-Host "Reusing existing DiligentEngine checkout at $engineRoot ($engineCommit)"
}

Write-Host 'Initializing DiligentCore ...'
git -C $engineRoot submodule update --init --depth 1 -- DiligentCore

$arguments = @('submodule', 'update', '--init', '--depth', '1', '--') + $requiredSubmodules
Write-Host "Initializing DiligentCore third-party submodules ..."
& git -C $coreRoot @arguments

if ($AcquireOnly)
{
	Write-Host 'AcquireOnly: sources pinned; skipping the CMake build.'
	return
}

Write-Host "Configuring DiligentCore (Vulkan-only, $Generator) ..."
# /GL (whole-program optimization) is disabled: the game links these static
# libraries into a non-LTCG executable, and mixing /GL objects breaks the link.
cmake -S $coreRoot -B $buildRoot -G $Generator -A x64 `
	-DDILIGENT_NO_DIRECT3D11=ON -DDILIGENT_NO_DIRECT3D12=ON `
	-DDILIGENT_NO_OPENGL=ON -DDILIGENT_NO_METAL=ON -DDILIGENT_BUILD_TESTS=OFF `
	-DDILIGENT_MSVC_RELEASE_COMPILE_OPTIONS=/arch:AVX2

# volk is a separate target in current DiligentCore; the engine's static library
# leaves the Vulkan loader entry points for the application to provide.
Write-Host 'Building Diligent-GraphicsEngineVk-static and volk (Release) ...'
cmake --build $buildRoot --config Release --target Diligent-GraphicsEngineVk-static volk -- /m /nologo /v:minimal

$engineLib = Join-Path $buildRoot 'Graphics\GraphicsEngineVulkan\Release\Diligent-GraphicsEngineVk-static.lib'
$volkLib = Join-Path $buildRoot 'ThirdParty\volk\Release\volk.lib'
foreach ($lib in @($engineLib, $volkLib))
{
	if (-not (Test-Path -LiteralPath $lib))
	{
		throw "Diligent build did not produce $lib"
	}
}
Write-Host "Diligent ready: $engineLib"
