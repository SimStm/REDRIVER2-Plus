[CmdletBinding()]
param(
	# FMOD Engine SDK root. Defaults to the gitignored dependencies tree, or $env:FMOD_DIR.
	[string] $FmodDir = $env:FMOD_DIR
)

# E04: verifies the FMOD Engine SDK used by the audio service.
#
# The SDK is login-gated at fmod.com, so this script never downloads it: it
# verifies an existing checkout, prints the detected version and fails with the
# exact manual acquisition steps. The SDK is an external dependency and lives in
# the gitignored `src_rebuild/dependencies/fmod`; never commit it, and ship only
# the runtime library (fmod.dll) with releases. Attribution and a startup logo
# are required by the FMOD license; see knowledge/rules/fmod-integration.md.

$ErrorActionPreference = 'Stop'

function ConvertFrom-FmodPackedVersion {
	param([uint32] $Packed)
	for ($shift = 0; $shift -lt 32; $shift += 4) {
		if ((($Packed -shr $shift) -band 15) -gt 9) {
			throw ('FMOD_VERSION 0x{0:X8} contains invalid BCD digits.' -f $Packed)
		}
	}
	# All fields contain decimal digits encoded as hex nibbles, including the
	# entire 16-bit product field (not just its low byte).
	$product = [int](('{0:X}' -f (($Packed -shr 16) -band 65535)))
	$minor = [int](('{0:X2}' -f (($Packed -shr 8) -band 255)))
	$patch = [int](('{0:X2}' -f ($Packed -band 255)))
	[pscustomobject]@{
		Product = $product; Minor = $minor; Patch = $patch
		Text = '{0}.{1:D2}.{2:D2}' -f $product, $minor, $patch
	}
}

function ConvertTo-FmodNormalizedDllVersion {
	param([string] $Version)
	$match = [regex]::Match($Version, '^(\d+)\.(\d+)\.(\d+)$')
	if (-not $match.Success) { throw "Unrecognized FMOD DLL ProductVersion: $Version" }
	'{0}.{1:D2}.{2:D2}' -f [int]$match.Groups[1].Value, [int]$match.Groups[2].Value, [int]$match.Groups[3].Value
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repositoryRoot 'src_rebuild'
if (-not $FmodDir) { $FmodDir = Join-Path $sourceRoot 'dependencies\fmod' }

$coreCandidates = @(
	(Join-Path $FmodDir 'core'),
	(Join-Path $FmodDir 'api\core')
)
$coreRoot = $coreCandidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'inc\fmod.h') } | Select-Object -First 1

if (-not $coreRoot) {
	Write-Host "FMOD Engine SDK not found under $FmodDir"
	Write-Host ''
	Write-Host 'Acquisition (manual; fmod.com requires an account):'
	Write-Host '  1. Sign in at https://www.fmod.com/download and download the current 2.03.x'
	Write-Host '     FMOD Engine installer for Windows.'
	Write-Host '  2. Install or extract it, then copy its api\core folder to:'
	Write-Host "       $FmodDir\core"
	Write-Host '     expected layout: core\inc\fmod.h, core\lib\x64\fmod_vc.lib, core\lib\x64\fmod.dll'
	Write-Host '  3. Re-run this script to verify the version and layout.'
	exit 1
}

# FMOD_VERSION packs the release bytes: 0x00020314 is 2.03.14.
$header = Join-Path $coreRoot 'inc\fmod_common.h'
$match = Select-String -LiteralPath $header -Pattern '#define\s+FMOD_VERSION\s+(0x[0-9A-Fa-f]+)' | Select-Object -First 1
if (-not $match) {
	Write-Host "Could not read FMOD_VERSION from $header"
	exit 1
}
$version = [Convert]::ToUInt32($match.Matches[0].Groups[1].Value.Substring(2), 16)
$decoded = ConvertFrom-FmodPackedVersion $version
$versionText = $decoded.Text

$lib = Join-Path $coreRoot 'lib\x64\fmod_vc.lib'
if (-not (Test-Path -LiteralPath $lib)) {
	Write-Host "FMOD SDK is missing $lib"
	Write-Host 'Copy the full api\core folder (inc + lib) from the installer so fmod.h and fmod_vc.lib are both present.'
	exit 1
}

$runtimeDll = Join-Path $coreRoot 'lib\x64\fmod.dll'
Write-Host "FMOD Engine SDK ready: version $versionText"
Write-Host "  headers:  $(Join-Path $coreRoot 'inc')"
Write-Host "  library:  $lib"
if (Test-Path -LiteralPath $runtimeDll) {
	Write-Host "  runtime:  $runtimeDll"
} else {
	throw "FMOD SDK is missing $runtimeDll"
}
$dllVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($runtimeDll)
$actualDll = $dllVersion.ProductVersion
if ((ConvertTo-FmodNormalizedDllVersion $actualDll) -ne $versionText) { throw "FMOD header $versionText does not match DLL metadata $actualDll" }
Write-Host "  DLL metadata: $actualDll (matches header BCD version)"

if ($decoded.Product -ne 2 -or $decoded.Minor -ne 3) {
	Write-Host "WARNING: the project targets the 2.03 production line; $versionText may have layout/API differences." -ForegroundColor Yellow
}
Write-Host 'Reminder: never commit SDK contents; ship only fmod.dll plus the required attribution.'
