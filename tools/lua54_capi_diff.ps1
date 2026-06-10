# Differential C API sweep: builds tools/lua54_capi_diff.c twice (against the
# official Lua 5.4.8 sources and against the LuaJIT Lua 5.4 compat build),
# runs both probes and diffs the normalized output. Zero diff lines = pass.
#
# Usage: powershell -File tools\lua54_capi_diff.ps1
# Requires: MSYS2 ucrt64 gcc, official sources (LUA54_SRC_DIR or default path),
# and an up-to-date Lua 5.4 compat build (src/lua51.dll).

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$msys = 'H:\p4\gl_home_u4\pristine\ruby\msys64'
if (-not (Test-Path "$msys\ucrt64\bin\gcc.exe")) {
  Write-Error "gcc not found under $msys\ucrt64\bin"
}
$env:PATH = "$msys\ucrt64\bin;$msys\usr\bin;$env:PATH"

$srcdir = $env:LUA54_SRC_DIR
if (-not $srcdir) { $srcdir = 'H:\p4\gl_home_u4\pristine\tools\lua\source\lua-5.4.8' }
if (-not (Test-Path "$srcdir\lapi.c")) {
  Write-Error "official Lua 5.4.8 sources not found at $srcdir"
}

$tmp = Join-Path $repo 'tmp_capi_diff'
New-Item -ItemType Directory -Force $tmp | Out-Null

$core = Get-ChildItem "$srcdir\*.c" |
  Where-Object { $_.Name -notin @('lua.c', 'onelua.c', 'ltests.c') } |
  ForEach-Object { $_.FullName }

Write-Host '[1/4] building official probe...'
& gcc -O1 -I $srcdir "$repo\tools\lua54_capi_diff.c" @core `
  -o "$tmp\capi_official.exe" -lm
if ($LASTEXITCODE -ne 0) { Write-Error 'official probe build failed' }

Write-Host '[2/4] building compat probe...'
& gcc -O1 -DLUAJIT_ENABLE_LUA54COMPAT -I "$repo\src" -x c `
  "$repo\tools\lua54_capi_diff.c" -x none "$repo\src\lua51.dll" `
  -o "$repo\src\capi_compat.exe"
if ($LASTEXITCODE -ne 0) { Write-Error 'compat probe build failed' }

Write-Host '[3/4] running probes...'
& "$tmp\capi_official.exe" > "$tmp\official.txt" 2>&1
if ($LASTEXITCODE -ne 0) { Write-Error "official probe exited $LASTEXITCODE" }
& "$repo\src\capi_compat.exe" > "$tmp\compat.txt" 2>&1
if ($LASTEXITCODE -ne 0) { Write-Error "compat probe exited $LASTEXITCODE" }

$doneOff = Select-String -Path "$tmp\official.txt" -Pattern 'CAPI-DIFF-DONE'
$doneCmp = Select-String -Path "$tmp\compat.txt" -Pattern 'CAPI-DIFF-DONE'
if (-not $doneOff -or -not $doneCmp) {
  Write-Error 'probe did not run to completion'
}

Write-Host '[4/4] diffing...'
$off = Get-Content "$tmp\official.txt"
$cmp = Get-Content "$tmp\compat.txt"
$diff = Compare-Object $off $cmp
if ($diff) {
  $diff | Select-Object -First 40 | Format-Table -AutoSize | Out-String | Write-Host
  Write-Host ("CAPI DIFF FAIL: {0} differing lines (see {1})" -f `
    ($diff | Measure-Object).Count, $tmp)
  exit 1
}
Write-Host ("CAPI DIFF PASS: outputs byte-identical ({0})" -f $doneOff.Line.Trim())
exit 0
