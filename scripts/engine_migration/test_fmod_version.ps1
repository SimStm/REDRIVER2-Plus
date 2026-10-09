# Pure fixtures exercise the verifier's actual functions without loading an SDK
# or executing its acquisition/verification entry point.
$ErrorActionPreference = 'Stop'
$path = Join-Path (Split-Path -Parent $PSScriptRoot) 'fetch_fmod.ps1'
$tokens = $null; $errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Verifier syntax errors.' }
foreach ($name in @('ConvertFrom-FmodPackedVersion', 'ConvertTo-FmodNormalizedDllVersion')) {
	$function = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
	if (-not $function) { throw "Missing verifier function: $name" }
	. ([scriptblock]::Create($function.Extent.Text))
}
$checks = 0
foreach ($fixture in @(
	@{ Packed = [uint32]0x00020315; Expected = '2.03.15'; Dll = '2.3.15' },
	@{ Packed = [uint32]0x00021015; Expected = '2.10.15'; Dll = '2.10.15' },
	@{ Packed = [uint32]0x12341015; Expected = '1234.10.15'; Dll = '1234.10.15' }
)) {
	$result = ConvertFrom-FmodPackedVersion $fixture.Packed
	if ($result.Text -ne $fixture.Expected) { throw "Decode mismatch: $($result.Text) vs $($fixture.Expected)" }
	++$checks; Write-Host ('PASS: 0x{0:X8} -> {1}' -f $fixture.Packed, $result.Text)
	if ((ConvertTo-FmodNormalizedDllVersion $fixture.Dll) -ne $result.Text) { throw 'DLL normalization mismatch.' }
	++$checks; Write-Host "PASS: DLL $($fixture.Dll) normalizes to $($result.Text)"
}
foreach ($packed in @([uint32]0x00020A15, [uint32]0x0002031A, [uint32]0x12A40315)) {
	$expected = 'FMOD_VERSION 0x{0:X8} contains invalid BCD digits.' -f $packed
	$diagnostic = $null
	try { $null = ConvertFrom-FmodPackedVersion $packed } catch { $diagnostic = $_.Exception.Message }
	if ($diagnostic -ne $expected) { throw "Expected exact diagnostic '$expected', got '$diagnostic'" }
	++$checks; Write-Host "PASS: $diagnostic"
}
Write-Host "$checks checks, 0 failures"
