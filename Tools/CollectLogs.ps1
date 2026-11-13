$ErrorActionPreference = 'Continue'
$projectDir = Split-Path -Parent $PSScriptRoot
$logDir     = Join-Path $projectDir 'Saved\Logs'
$buildLog   = Join-Path $logDir 'UBT-Build.txt'
$gameLog    = Join-Path $logDir 'Paleotrench.log'
$prepareLog = Join-Path $logDir 'M1-Prepare.txt'
$summary    = Join-Path $logDir 'Summary.txt'

$out = New-Object System.Collections.Generic.List[string]
$out.Add('=== Paleotrench log summary ===')
$out.Add("Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')")

$commit = ''
try { $commit = (& git -C $projectDir rev-parse --short HEAD 2>$null) } catch { }
$out.Add("Commit: $commit")
$out.Add('')

$out.Add('--- Build errors (UBT-Build.txt) ---')
if (Test-Path $buildLog) {
    $errors = @(Select-String -Path $buildLog -Pattern 'error|fatal|undefined|unresolved' | Select-Object -First 80)
    if ($errors.Count -eq 0) {
        $out.Add('(no error lines found)')
    } else {
        foreach ($e in $errors) { $out.Add($e.Line) }
    }
    $out.Add('')
    $out.Add('--- Last 30 lines of build log ---')
    foreach ($line in (Get-Content $buildLog -Tail 30)) { $out.Add($line) }
} else {
    $out.Add('(no build log yet - run Tools\Build.bat first)')
}
$out.Add('')

$out.Add('--- Game log: errors, warnings and Paleotrench lines ---')
if (Test-Path $gameLog) {
    $hits = @(Select-String -Path $gameLog -Pattern 'Error:|Warning:|LogPaleotrench|Fatal|Assertion|Ensure' | Select-Object -Last 120)
    if ($hits.Count -eq 0) {
        $out.Add('(nothing notable)')
    } else {
        foreach ($h in $hits) { $out.Add($h.Line) }
    }
} else {
    $out.Add('(no game log yet - open the project once)')
}

$out.Add('')
$out.Add('--- M1 preparation log ---')
if (Test-Path $prepareLog) {
    $hits = @(Select-String -Path $prepareLog -Pattern 'Error:|Warning:|Traceback|RuntimeError|LogPaleotrench|PALEOTRENCH M1' | Select-Object -Last 80)
    foreach ($h in $hits) { $out.Add($h.Line) }
    $out.Add('--- Last 20 lines of M1 preparation ---')
    foreach ($line in (Get-Content $prepareLog -Tail 20)) { $out.Add($line) }
} else {
    $out.Add('(no M1 preparation log yet)')
}

if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }
$out | Set-Content -Path $summary -Encoding UTF8
try { $out -join "`r`n" | Set-Clipboard } catch { }

Write-Host ''
Write-Host "Summary written to: $summary"
Write-Host 'It has also been copied to your clipboard - paste it into the chat with Ctrl+V.'
