# Differential sweep driver: runs the lua54_diff_probe*.lua batteries on the
# official Lua 5.4 interpreter and the LuaJIT Lua 5.4 compat build, then
# compares the outputs. Known accepted differences are filtered.
#
# Usage:
#   powershell -File tools\lua54_diff_sweep.ps1 [-Official <lua54.exe>] [-Compat <luajit.exe>]
#
# The compat build must be a LUAJIT_ENABLE_LUA54COMPAT build (build.bat lua54build).

param(
  [string]$Official = $env:LUA54_REF_EXE,
  [string]$Compat = "$PSScriptRoot\..\src\luajit.exe"
)

$ErrorActionPreference = 'Stop'

if (-not $Official) {
  $candidates = @(
    'H:\p4\gl_home_u4\pristine\tools\lua\lua5.4.8\lua54.exe'
  )
  $Official = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Official -or -not (Test-Path $Official)) {
  Write-Error "official Lua 5.4 interpreter not found; set LUA54_REF_EXE"
}
if (-not (Test-Path $Compat)) {
  Write-Error "compat luajit.exe not found at $Compat (run build.bat lua54build)"
}

$ver = & $Compat -e "io.write(_VERSION, '/', tostring(require('jit').lua54compat))"
if ($ver -ne 'Lua 5.4/true') {
  Write-Error "compat exe is not a Lua 5.4 compat build: $ver"
}

# Probe lines that are known, documented differences (see the probe sources).
$allow = @(
  '#op:-0.0 - 0 '  # official constant-fold quirk: x - 0 compiles to OP_ADDI
)

$tmp = Join-Path $env:TEMP 'lua54_diff_sweep'
New-Item -ItemType Directory -Force $tmp | Out-Null

$failed = 0
foreach ($probe in @('lua54_diff_probe.lua', 'lua54_diff_probe2.lua',
                     'lua54_diff_probe3.lua')) {
  $script = Join-Path $PSScriptRoot $probe
  $offOut = Join-Path $tmp ($probe + '.official.txt')
  $cmpOut = Join-Path $tmp ($probe + '.compat.txt')
  & $Official $script 1> $offOut 2> $null
  if ($LASTEXITCODE -ne 0) { Write-Error "official run failed for $probe" }
  & $Compat $script 1> $cmpOut 2> $null
  if ($LASTEXITCODE -ne 0) { Write-Error "compat run failed for $probe" }

  $a = [System.IO.File]::ReadAllLines($offOut)
  $b = [System.IO.File]::ReadAllLines($cmpOut)
  $diffs = @()
  $n = [Math]::Max($a.Count, $b.Count)
  for ($i = 0; $i -lt $n; $i++) {
    $la = if ($i -lt $a.Count) { $a[$i] } else { '<missing>' }
    $lb = if ($i -lt $b.Count) { $b[$i] } else { '<missing>' }
    if ($la -cne $lb) {
      $known = $false
      foreach ($p in $allow) {
        if ($la.StartsWith($p) -and $lb.StartsWith($p)) { $known = $true; break }
      }
      if (-not $known) {
        $diffs += "line $($i+1):`n  official: $la`n  compat:   $lb"
      }
    }
  }
  if ($diffs.Count -gt 0) {
    Write-Host "FAIL $probe ($($diffs.Count) unexpected diffs)" -ForegroundColor Red
    $diffs | Select-Object -First 20 | ForEach-Object { Write-Host $_ }
    $failed += $diffs.Count
  } else {
    Write-Host "OK   $probe ($($a.Count) lines, allowlisted diffs only)"
  }
}

if ($failed -gt 0) {
  Write-Error "lua54 diff sweep found $failed unexpected differences"
}
Write-Host "lua54 diff sweep OK"
