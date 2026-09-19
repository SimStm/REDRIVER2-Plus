$openal_ver = '1.23.1'
$sdl2_ver = '2.30.2'

$windows_premake_url = 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-windows.zip'
$windows_jpeg_url = 'http://www.ijg.org/files/jpegsr9d.zip'
$windows_openal_url = 'https://github.com/kcat/openal-soft/releases/download/' + $openal_ver + '/openal-soft-' + $openal_ver + '-bin.zip'
$windows_sdl2_url = 'https://github.com/libsdl-org/SDL/releases/download/release-' + $sdl2_ver + '/SDL2-devel-' + $sdl2_ver + '-VC.zip'

$project_folder = Join-Path $PSScriptRoot 'src_rebuild'
$dependency_folder = Join-Path $project_folder 'dependencies'

$premake_exe = Join-Path $project_folder 'premake5.exe'
$windows_jpeg_dir = Join-Path $dependency_folder 'jpeg-9d'
$windows_openal_dir = Join-Path $dependency_folder ('openal-soft-' + $openal_ver + '-bin')
$windows_sdl2_dir = Join-Path $dependency_folder ('SDL2-' + $sdl2_ver)

# Download and extract a dependency only when it is missing, so re-running the
# script is cheap and does not touch an existing tree.
function Get-Dependency
{
	param(
		[Parameter(Mandatory = $true)][string]$Url,
		[Parameter(Mandatory = $true)][string]$Archive,
		[Parameter(Mandatory = $true)][string]$Target,
		[Parameter(Mandatory = $true)][string]$Destination
	)

	if (Test-Path -LiteralPath $Target)
	{
		Write-Host "Already present: $Target"
		return
	}

	Write-Host "Downloading $Url"
	Invoke-WebRequest -Uri $Url -OutFile $Archive
	New-Item -ItemType Directory -Path $Destination -Force | Out-Null
	Expand-Archive -Path $Archive -DestinationPath $Destination -Force
}

Get-Dependency -Url $windows_premake_url -Archive (Join-Path $PSScriptRoot 'PREMAKE.zip') -Target $premake_exe -Destination $project_folder
Get-Dependency -Url $windows_sdl2_url -Archive (Join-Path $PSScriptRoot 'SDL2.zip') -Target $windows_sdl2_dir -Destination $dependency_folder
Get-Dependency -Url $windows_openal_url -Archive (Join-Path $PSScriptRoot 'OPENAL.zip') -Target $windows_openal_dir -Destination $dependency_folder
Get-Dependency -Url $windows_jpeg_url -Archive (Join-Path $PSScriptRoot 'JPEG.zip') -Target $windows_jpeg_dir -Destination $dependency_folder

# The IJG source ships jconfig.vc; the project builds against jconfig.h.
$jconfigVc = Join-Path $windows_jpeg_dir 'jconfig.vc'
$jconfigH = Join-Path $windows_jpeg_dir 'jconfig.h'
if ((Test-Path -LiteralPath $jconfigVc) -and -not (Test-Path -LiteralPath $jconfigH))
{
	Rename-Item -LiteralPath $jconfigVc -NewName 'jconfig.h'
}

# The build uses the project PsyCross fork; make sure the submodule is present.
git -C $PSScriptRoot submodule update --init --recursive

$env:SDL2_DIR = 'dependencies\SDL2-' + $sdl2_ver
$env:OPENAL_DIR = 'dependencies\openal-soft-' + $openal_ver + '-bin'
$env:JPEG_DIR = 'dependencies\jpeg-9d'

Push-Location $project_folder
try
{
	& $premake_exe vs2022
}
finally
{
	Pop-Location
}

$solution = Join-Path $project_folder 'build\REDRIVER2.sln'
Write-Host ''
Write-Host "Solution generated: $solution"
Write-Host 'Open it, select Release_dev | x64 and build the REDRIVER2 project.'

# Open the generated solution.
& $solution
