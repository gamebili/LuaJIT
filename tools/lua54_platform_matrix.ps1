[CmdletBinding()]
param(
  [ValidateSet("all", "pc", "android", "probe")]
  [string]$Target = "all",
  [switch]$RequireAll,
  [string]$Make = "make"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RepoRoot

$Results = New-Object System.Collections.Generic.List[object]

function Add-Result {
  param(
    [string]$TargetName,
    [string]$Status,
    [string]$Detail
  )
  $Results.Add([pscustomobject]@{
    Target = $TargetName
    Status = $Status
    Detail = $Detail
  }) | Out-Null
}

function Add-LocalMsysPath {
  $candidates = @()
  if ($env:LUAJIT_MSYS2_UCRT64_BIN) {
    $candidates += $env:LUAJIT_MSYS2_UCRT64_BIN
  }
  $candidates += "D:\p4_gl2\pristine\ruby\Ruby33-x64\msys64\ucrt64\bin"

  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path $candidate)) {
      if (($env:PATH -split ";") -notcontains $candidate) {
        $env:PATH = "$candidate;$env:PATH"
      }
      return
    }
  }
}

function Invoke-Checked {
  param(
    [string]$File,
    [string[]]$Arguments
  )
  Write-Host ("[run] {0} {1}" -f $File, ($Arguments -join " "))
  & $File @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw ("command failed with exit code {0}: {1} {2}" -f
      $LASTEXITCODE, $File, ($Arguments -join " "))
  }
}

function Invoke-PcX64 {
  Add-LocalMsysPath
  if (-not (Get-Command $Make -ErrorAction SilentlyContinue)) {
    Add-Result "pc-x64-default" "FAIL" "GNU make not found. Put MSYS2/UCRT64 bin on PATH or pass -Make."
    Add-Result "pc-x64-lua54compat" "FAIL" "GNU make not found. Put MSYS2/UCRT64 bin on PATH or pass -Make."
    return
  }

  try {
    Invoke-Checked $Make @("clean")
    Invoke-Checked $Make @()
    Invoke-Checked (Join-Path $RepoRoot "src\luajit.exe") @("test/smoke.lua", "default")
    Add-Result "pc-x64-default" "PASS" "Built and ran test/smoke.lua default."
  } catch {
    Add-Result "pc-x64-default" "FAIL" $_.Exception.Message
    return
  }

  try {
    Invoke-Checked $Make @("clean")
    Invoke-Checked $Make @("XCFLAGS=-DLUAJIT_ENABLE_LUA54COMPAT")
    Invoke-Checked (Join-Path $RepoRoot "src\luajit.exe") @("test/smoke.lua", "lua54compat")
    Add-Result "pc-x64-lua54compat" "PASS" "Built with LUAJIT_ENABLE_LUA54COMPAT and ran Lua 5.4 smoke."
  } catch {
    Add-Result "pc-x64-lua54compat" "FAIL" $_.Exception.Message
  }
}

function Get-EnvPath {
  param([string[]]$Names)
  foreach ($name in $Names) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value -and (Test-Path $value)) {
      return $value
    }
  }
  return $null
}

function Invoke-AndroidArm64 {
  Add-LocalMsysPath
  if (-not (Get-Command $Make -ErrorAction SilentlyContinue)) {
    Add-Result "android-arm64-lua54compat" "FAIL" "GNU make not found. Put MSYS2/UCRT64 bin on PATH or pass -Make."
    return
  }

  $ndkRoot = Get-EnvPath @("ANDROID_NDK_ROOT", "ANDROID_NDK_HOME", "NDK_ROOT")
  if (-not $ndkRoot) {
    Add-Result "android-arm64-lua54compat" "SKIP" "ANDROID_NDK_ROOT/ANDROID_NDK_HOME/NDK_ROOT is not set to an existing NDK."
    return
  } else {
    $clang = Get-ChildItem -Path (Join-Path $ndkRoot "toolchains\llvm\prebuilt") `
      -Recurse -Filter "aarch64-linux-android*-clang.cmd" -ErrorAction SilentlyContinue |
      Select-Object -First 1
    if (-not $clang) {
      Add-Result "android-arm64-lua54compat" "SKIP" "NDK detected at $ndkRoot but no aarch64-linux-android*-clang.cmd was found."
      return
    }
  }

  $toolBin = Split-Path -Parent $clang.FullName
  $ar = Join-Path $toolBin "llvm-ar.exe"
  $strip = Join-Path $toolBin "llvm-strip.exe"
  $readelf = Join-Path $toolBin "llvm-readelf.exe"
  if (-not (Test-Path $ar)) {
    Add-Result "android-arm64-lua54compat" "SKIP" "llvm-ar.exe not found beside $($clang.FullName)."
    return
  }

  try {
    Invoke-Checked $Make @("clean")
    Invoke-Checked $Make @(
      "HOST_CC=gcc",
      "TARGET_SYS=Linux",
      "CC=$($clang.FullName)",
      "TARGET_AR=$ar rcus",
      "TARGET_STRIP=$strip",
      "BUILDMODE=static",
      "XCFLAGS=-DLUAJIT_ENABLE_LUA54COMPAT"
    )
    $exe = Join-Path $RepoRoot "src\luajit"
    $lib = Join-Path $RepoRoot "src\libluajit.a"
    if (-not (Test-Path $exe) -or -not (Test-Path $lib)) {
      throw "expected Android artifacts src/luajit and src/libluajit.a were not created"
    }
    if (Test-Path $readelf) {
      $header = & $readelf -h $exe 2>&1 | Out-String
      if ($LASTEXITCODE -ne 0 -or $header -notmatch "AArch64") {
        throw "llvm-readelf did not confirm an AArch64 executable"
      }
    }
    Add-Result "android-arm64-lua54compat" "PASS" "Built Lua 5.4 compat Android ARM64 static artifacts with NDK $ndkRoot."
  } catch {
    Add-Result "android-arm64-lua54compat" "FAIL" $_.Exception.Message
  }
}

function Add-PlatformProbes {

  $isWindowsHost = [System.Environment]::OSVersion.Platform -eq "Win32NT"
  if ($isWindowsHost) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "iOS ARM64 build requires macOS Xcode/xcrun; this host is Windows."
  } elseif (Get-Command xcrun -ErrorAction SilentlyContinue) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun detected; iOS make args and artifact validation still need wiring."
  } else {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun not found."
  }

  if (Get-Command emcc -ErrorAction SilentlyContinue) {
    Add-Result "emscripten-wasm-lua54compat" "SKIP" "emcc detected; LuaJIT has no wasm JIT backend here, so interpreter/wasm build path still needs implementation."
  } else {
    Add-Result "emscripten-wasm-lua54compat" "SKIP" "emcc not found; Emscripten build path also needs interpreter/wasm support."
  }
}

if ($Target -eq "all" -or $Target -eq "pc") {
  Invoke-PcX64
}

if ($Target -eq "all" -or $Target -eq "android") {
  Invoke-AndroidArm64
}

if ($Target -eq "all" -or $Target -eq "probe") {
  Add-PlatformProbes
}

$Results | Format-Table -AutoSize

$failed = @($Results | Where-Object { $_.Status -eq "FAIL" })
$skipped = @($Results | Where-Object { $_.Status -eq "SKIP" })
if ($failed.Count -gt 0 -or ($RequireAll -and $skipped.Count -gt 0)) {
  exit 1
}
