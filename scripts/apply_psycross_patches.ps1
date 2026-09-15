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

# Windows PowerShell 5.1 can hang forever when a native command's stderr is
# redirected to $null while $ErrorActionPreference is 'Stop' and the command
# writes errors: a failing `git apply` reports every rejected file. Capture the
# combined stream into a variable instead of discarding stderr, and relax the
# preference around the call so native stderr cannot become terminating.
function Invoke-GitApply([string[]] $arguments)
{
	$previous = $ErrorActionPreference
	$ErrorActionPreference = 'Continue'
	try
	{
		$output = & git -C $submodulePath apply @arguments 2>&1
		$code = $LASTEXITCODE
	}
	finally
	{
		$ErrorActionPreference = $previous
	}

	return [pscustomobject]@{
		ExitCode = $code
		Output = ($output | Out-String).Trim()
	}
}

$reverseCheck = Invoke-GitApply @('--reverse', '--check', $patchPath)
if ($reverseCheck.ExitCode -eq 0)
{
	Write-Host 'PsyCross developer-overlay patch is already applied.'
	exit 0
}

$forwardCheck = Invoke-GitApply @('--check', $patchPath)
if ($forwardCheck.ExitCode -ne 0)
{
	if ($forwardCheck.Output) { Write-Host $forwardCheck.Output }
	throw 'PsyCross patch cannot be applied cleanly. Inspect the submodule status and patch base revision.'
}

$applyResult = Invoke-GitApply @($patchPath)
if ($applyResult.ExitCode -ne 0)
{
	if ($applyResult.Output) { Write-Host $applyResult.Output }
	throw 'PsyCross patch application failed.'
}

Write-Host 'Applied PsyCross developer-overlay patch.'
