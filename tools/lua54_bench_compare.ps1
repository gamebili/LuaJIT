# Runs tools/lua54_bench.lua on three runtimes sequentially and reports
# wall time, Lua-heap allocation/retention and process peak working set:
#   1. LuaJIT Lua 5.4 compat, JIT on (src/luajit.exe)
#   2. LuaJIT Lua 5.4 compat, JIT off (src/luajit.exe -joff)
#   3. official Lua 5.4.8 (lua54.exe)
# Usage: powershell -File tools\lua54_bench_compare.ps1

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$bench = Join-Path $repo 'tools\lua54_bench.lua'
$official = 'H:\p4\gl_home_u4\pristine\tools\lua\lua5.4.8\lua54.exe'
$luajit = Join-Path $repo 'src\luajit.exe'

$configs = @(
  @{ name = 'jit_on';  exe = $luajit;   args = @($bench) },
  @{ name = 'jit_off'; exe = $luajit;   args = @('-joff', $bench) },
  @{ name = 'lua548';  exe = $official; args = @($bench) }
)

$outdir = Join-Path $repo 'tmp_bench'
New-Item -ItemType Directory -Force $outdir | Out-Null

foreach ($cfg in $configs) {
  $out = Join-Path $outdir ($cfg.name + '.txt')
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $cfg.exe
  # Windows PowerShell 5.1 lacks ProcessStartInfo.ArgumentList
  $psi.Arguments = ($cfg.args | ForEach-Object { '"' + $_ + '"' }) -join ' '
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.UseShellExecute = $false
  $psi.WorkingDirectory = $repo
  $p = [System.Diagnostics.Process]::Start($psi)
  # PeakWorkingSet64 is unreadable after exit on Windows PowerShell 5.1,
  # so poll it while the async output reads drain the pipes.
  $outTask = $p.StandardOutput.ReadToEndAsync()
  $errTask = $p.StandardError.ReadToEndAsync()
  $peak = 0
  while (-not $p.HasExited) {
    try { $p.Refresh(); $peak = [math]::Max($peak, $p.PeakWorkingSet64) } catch {}
    Start-Sleep -Milliseconds 50
  }
  $p.WaitForExit()
  $stdout = $outTask.Result
  $stderr = $errTask.Result
  $peakMB = [math]::Round($peak / 1MB, 1)
  if ($p.ExitCode -ne 0 -or $stdout -notmatch 'BENCH-DONE') {
    Write-Host $stderr
    Write-Error ("{0} failed (exit {1})" -f $cfg.name, $p.ExitCode)
  }
  ($stdout + ("PEAK_WS_MB {0}`n" -f $peakMB)) | Set-Content $out
  Write-Host ("{0} done (peak working set {1} MB)" -f $cfg.name, $peakMB)
}

Write-Host "results in $outdir"
