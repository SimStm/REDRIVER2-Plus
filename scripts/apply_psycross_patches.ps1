[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$submodulePath = Join-Path $repositoryRoot 'src_rebuild\PsyCross'
$patchPath = Join-Path $repositoryRoot 'patches\psycross\developer-overlay.patch'
$expectedCommit = 'e56e4cde1c2b8a15e0d4e38b26cdd9202e0d17e6'

if (-not (Test-Path -LiteralPath (Join-Path $submodulePath '.git')))
{
	throw 'PsyCross is not initialised. Run: git submodule update --init --recursive'
}

if (-not (Test-Path -LiteralPath $patchPath))
{
	throw "PsyCross patch not found: $patchPath"
}

$currentCommit = (& git -C $submodulePath rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $currentCommit -ne $expectedCommit)
{
	throw "PsyCross must be at $expectedCommit; found $currentCommit. Reinitialise or rebase the patch before applying it."
}

& git -C $submodulePath apply --reverse --check $patchPath 2>$null
if ($LASTEXITCODE -eq 0)
{
	Write-Host 'PsyCross developer-overlay patch is already applied.'
	exit 0
}

& git -C $submodulePath apply --check $patchPath
if ($LASTEXITCODE -ne 0)
{
	throw 'PsyCross patch cannot be applied cleanly. Inspect the submodule status and patch base revision.'
}

& git -C $submodulePath apply $patchPath
if ($LASTEXITCODE -ne 0)
{
	throw 'PsyCross patch application failed.'
}

Write-Host 'Applied PsyCross developer-overlay patch.'
