[CmdletBinding()]
param(
  [ValidateSet("all", "pc", "android", "ios", "emscripten", "probe")]
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
  $roots = @()
  foreach ($name in @("LUAJIT_MSYS2_ROOT", "MSYS_ROOT")) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value -and (Test-Path $value)) {
      $roots += $value
    }
  }
  $roots += "H:\p4\gl_home_u4\pristine\ruby\msys64"
  $roots += "H:\p4\gl_home_u4\pristine\msys64"
  $roots += "D:\p4_gl2\pristine\ruby\Ruby33-x64\msys64"

  foreach ($root in $roots) {
    if (-not ($root -and (Test-Path $root))) {
      continue
    }
    foreach ($subdir in @("usr\bin", "ucrt64\bin", "mingw64\bin")) {
      $candidate = Join-Path $root $subdir
      if ($candidate -and (Test-Path $candidate) -and
          (($env:PATH -split ";") -notcontains $candidate)) {
        $env:PATH = "$candidate;$env:PATH"
      }
    }
    return
  }

  if ($env:LUAJIT_MSYS2_UCRT64_BIN -and
      (Test-Path $env:LUAJIT_MSYS2_UCRT64_BIN) -and
      (($env:PATH -split ";") -notcontains $env:LUAJIT_MSYS2_UCRT64_BIN)) {
    $env:PATH = "$env:LUAJIT_MSYS2_UCRT64_BIN;$env:PATH"
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

function Assert-PeAmd64Artifact {
  param([string]$Artifact)

  if (-not (Test-Path $Artifact)) {
    throw "expected artifact was not created: $Artifact"
  }

  $fs = [System.IO.File]::OpenRead($Artifact)
  try {
    $reader = New-Object System.IO.BinaryReader($fs)
    if ($reader.ReadUInt16() -ne 0x5a4d) {
      throw "artifact is not a PE executable: $Artifact"
    }
    [void]$fs.Seek(0x3c, [System.IO.SeekOrigin]::Begin)
    $peOffset = $reader.ReadInt32()
    if ($peOffset -lt 0 -or $peOffset -gt ($fs.Length - 6)) {
      throw "artifact has an invalid PE header offset: $Artifact"
    }
    [void]$fs.Seek($peOffset, [System.IO.SeekOrigin]::Begin)
    if ($reader.ReadUInt32() -ne 0x00004550) {
      throw "artifact has an invalid PE signature: $Artifact"
    }
    $machine = $reader.ReadUInt16()
    if ($machine -ne 0x8664) {
      throw ("expected AMD64 PE machine 0x8664, got 0x{0:x4}: {1}" -f $machine, $Artifact)
    }
    return "PE machine AMD64 (0x8664)"
  } finally {
    $fs.Dispose()
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
    $exe = Join-Path $RepoRoot "src\luajit.exe"
    $arch = Assert-PeAmd64Artifact $exe
    Invoke-Checked $exe @("test/smoke.lua", "default")
    Add-Result "pc-x64-default" "PASS" "Built $arch artifact and ran test/smoke.lua default."
  } catch {
    Add-Result "pc-x64-default" "FAIL" $_.Exception.Message
    return
  }

  try {
    Invoke-Checked $Make @("clean")
    Invoke-Checked $Make @("XCFLAGS=-DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2")
    $exe = Join-Path $RepoRoot "src\luajit.exe"
    $arch = Assert-PeAmd64Artifact $exe
    Invoke-Checked $exe @("test/smoke.lua", "lua54compat")
    Add-Result "pc-x64-lua54compat" "PASS" "Built Lua 5.4 compat dual-number $arch artifact and ran Lua 5.4 smoke."
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

function Get-AdbPath {
  $cmd = Get-Command adb -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }
  $candidates = @()
  if ($env:ANDROID_HOME) {
    $candidates += (Join-Path $env:ANDROID_HOME "platform-tools\adb.exe")
  }
  if ($env:ANDROID_SDK_ROOT) {
    $candidates += (Join-Path $env:ANDROID_SDK_ROOT "platform-tools\adb.exe")
  }
  $candidates += (Join-Path $env:LOCALAPPDATA "Android\Sdk\platform-tools\adb.exe")
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path $candidate)) {
      return $candidate
    }
  }
  return $null
}

function Get-EmccPath {
  $cmd = Get-Command emcc -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }
  $cmd = Get-Command emcc.bat -ErrorAction SilentlyContinue
  if ($cmd) {
    return $cmd.Source
  }

  $candidates = @()
  if ($env:EMCC -and (Test-Path $env:EMCC)) {
    $candidates += $env:EMCC
  }
  if ($env:EMSCRIPTEN) {
    $candidates += (Join-Path $env:EMSCRIPTEN "emcc.bat")
    $candidates += (Join-Path $env:EMSCRIPTEN "emcc")
  }
  if ($env:EMSDK) {
    $candidates += (Join-Path $env:EMSDK "upstream\emscripten\emcc.bat")
    $candidates += (Join-Path $env:EMSDK "upstream\emscripten\emcc")
  }

  # This Windows workspace commonly carries an engine-local emsdk for HTML5.
  # Probe it explicitly so platform status reports the real LuaJIT/wasm
  # blocker instead of a PATH-only "emcc not found" result.
  $candidates += "D:\p4_gl2\PG2\Engine\UE_TestGL260107\Engine\Platforms\HTML5\Build\emsdk\emsdk-4.0.3\upstream\emscripten\emcc.bat"

  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path $candidate)) {
      return (Resolve-Path $candidate).Path
    }
  }
  return $null
}

function Invoke-EmscriptenArchProbe {
  param(
    [string]$Emcc,
    [string]$TargetTriple
  )

  $oldErrorActionPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  try {
    $output = (& $Emcc -target $TargetTriple `
      -DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2 `
      -DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_FFI `
      -I (Join-Path $RepoRoot "src") -E (Join-Path $RepoRoot "src\lj_arch.h") `
      -dM 2>&1 | Out-String)
    $exitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $oldErrorActionPreference
  }

  # Treat only LuaJIT's own target-selection errors as an expected platform
  # gap. Other emcc/preprocessor failures should stay red in the matrix.
  $knownBackendMissing = $exitCode -ne 0 -and
    ($output -match "Architecture not supported" -or
     $output -match "No target architecture defined" -or
     $output -match "No support for this number mode on this architecture")

  $summary = $output.Trim()
  if ($summary.Length -gt 700) {
    $summary = $summary.Substring(0, 700) + "..."
  }

  return [pscustomobject]@{
    Target = $TargetTriple
    ExitCode = $exitCode
    KnownBackendMissing = $knownBackendMissing
    Summary = $summary
  }
}

function Get-FirstAdbDevice {
  param([string]$Adb)
  $lines = & $Adb devices 2>$null
  if ($LASTEXITCODE -ne 0) {
    return $null
  }
  foreach ($line in $lines) {
    if ($line -match "^(\S+)\s+device$") {
      return $matches[1]
    }
  }
  return $null
}

function Invoke-AndroidDeviceSmoke {
  param([string]$Artifact)
  $adb = Get-AdbPath
  if (-not $adb) {
    Add-Result "android-arm64-device-smoke" "SKIP" "adb not found; set ANDROID_HOME/ANDROID_SDK_ROOT or put adb on PATH."
    return
  }

  $device = Get-FirstAdbDevice $adb
  if (-not $device) {
    Add-Result "android-arm64-device-smoke" "SKIP" "adb found at $adb but no online device is listed by 'adb devices'."
    return
  }

  $remoteDir = "/data/local/tmp/luajit-lua54-matrix"
  try {
    Invoke-Checked $adb @("-s", $device, "shell", "rm", "-rf", $remoteDir)
    Invoke-Checked $adb @("-s", $device, "shell", "mkdir", "-p", $remoteDir)
    Invoke-Checked $adb @("-s", $device, "push", $Artifact, "$remoteDir/luajit")
    Invoke-Checked $adb @("-s", $device, "push", (Join-Path $RepoRoot "test\smoke.lua"), "$remoteDir/smoke.lua")
    # The Lua 5.4 smoke also checks LuaJIT tooling such as require("jit.dump").
    # Push the bundled Lua modules so target-device runs match the PC layout.
    Invoke-Checked $adb @("-s", $device, "push", (Join-Path $RepoRoot "src\jit"), "$remoteDir/jit")
    Invoke-Checked $adb @("-s", $device, "shell", "chmod", "755", "$remoteDir/luajit")
    Invoke-Checked $adb @("-s", $device, "shell", "cd $remoteDir && ./luajit smoke.lua lua54compat")
    Add-Result "android-arm64-device-smoke" "PASS" "Ran test/smoke.lua lua54compat on Android device $device."
  } catch {
    Add-Result "android-arm64-device-smoke" "FAIL" $_.Exception.Message
  }
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
      "XCFLAGS=-DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2"
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
    Invoke-AndroidDeviceSmoke $exe
  } catch {
    Add-Result "android-arm64-lua54compat" "FAIL" $_.Exception.Message
  }
}

function Invoke-IosArm64 {
  $isWindowsHost = [System.Environment]::OSVersion.Platform -eq "Win32NT"
  if ($isWindowsHost) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "iOS ARM64 build requires macOS Xcode/xcrun; this host is Windows."
    return
  }
  if (-not (Get-Command xcrun -ErrorAction SilentlyContinue)) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun not found."
    return
  }
  Add-LocalMsysPath
  if (-not (Get-Command $Make -ErrorAction SilentlyContinue)) {
    Add-Result "ios-arm64-lua54compat" "FAIL" "GNU make not found. Put it on PATH or pass -Make."
    return
  }

  $sdkPath = (& xcrun --sdk iphoneos --show-sdk-path 2>$null | Select-Object -First 1)
  if ($LASTEXITCODE -ne 0 -or -not $sdkPath -or -not (Test-Path $sdkPath)) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun is present but iphoneos SDK path was not found."
    return
  }
  $clang = (& xcrun --sdk iphoneos --find clang 2>$null | Select-Object -First 1)
  if ($LASTEXITCODE -ne 0 -or -not $clang -or -not (Test-Path $clang)) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun is present but iphoneos clang was not found."
    return
  }
  $ar = (& xcrun --sdk iphoneos --find ar 2>$null | Select-Object -First 1)
  if ($LASTEXITCODE -ne 0 -or -not $ar -or -not (Test-Path $ar)) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun is present but iphoneos ar was not found."
    return
  }
  $strip = (& xcrun --sdk iphoneos --find strip 2>$null | Select-Object -First 1)
  if ($LASTEXITCODE -ne 0 -or -not $strip -or -not (Test-Path $strip)) {
    Add-Result "ios-arm64-lua54compat" "SKIP" "xcrun is present but iphoneos strip was not found."
    return
  }

  $lipo = Get-Command lipo -ErrorAction SilentlyContinue
  $minVersion = if ($env:IPHONEOS_DEPLOYMENT_TARGET) {
    $env:IPHONEOS_DEPLOYMENT_TARGET
  } else {
    "12.0"
  }
  $targetFlags = "-arch arm64 -isysroot $sdkPath -miphoneos-version-min=$minVersion"

  try {
    Invoke-Checked $Make @("clean")
    Invoke-Checked $Make @(
      "HOST_CC=cc",
      "TARGET_SYS=iOS",
      "CC=$clang",
      "TARGET_AR=$ar rcus",
      "TARGET_STRIP=$strip",
      "TARGET_FLAGS=$targetFlags",
      "BUILDMODE=static",
      "XCFLAGS=-DLUAJIT_ENABLE_LUA54COMPAT -DLUAJIT_NUMMODE=2"
    )
    $lib = Join-Path $RepoRoot "src\libluajit.a"
    if (-not (Test-Path $lib)) {
      throw "expected iOS artifact src/libluajit.a was not created"
    }
    if ($lipo) {
      $info = (& $lipo.Source -info $lib 2>&1 | Out-String)
      if ($LASTEXITCODE -ne 0 -or $info -notmatch "arm64") {
        throw "lipo did not confirm arm64 in src/libluajit.a: $info"
      }
    }
    Add-Result "ios-arm64-lua54compat" "PASS" "Built Lua 5.4 compat iOS ARM64 static lib with iphoneos SDK $sdkPath; runtime smoke still needs an iOS device/app harness."
  } catch {
    Add-Result "ios-arm64-lua54compat" "FAIL" $_.Exception.Message
  }
}

function Invoke-EmscriptenWasm {
  $emcc = Get-EmccPath
  if ($emcc) {
    $version = (& $emcc --version 2>$null | Select-Object -First 1)
    if (-not $version) {
      $version = "emcc detected at $emcc"
    }

    $probes = @(
      Invoke-EmscriptenArchProbe $emcc "wasm32-unknown-emscripten"
      Invoke-EmscriptenArchProbe $emcc "wasm64-unknown-emscripten"
    )
    $unexpectedFailures = @($probes | Where-Object { $_.ExitCode -ne 0 -and -not $_.KnownBackendMissing })
    $unexpectedSuccesses = @($probes | Where-Object { $_.ExitCode -eq 0 })

    if ($unexpectedFailures.Count -gt 0) {
      $details = ($unexpectedFailures | ForEach-Object {
        "{0}: {1}" -f $_.Target, $_.Summary
      }) -join " | "
      Add-Result "emscripten-wasm-lua54compat" "FAIL" "emcc probe failed before the expected LuaJIT target-backend error: $details"
    } elseif ($unexpectedSuccesses.Count -gt 0) {
      $targets = ($unexpectedSuccesses | ForEach-Object { $_.Target }) -join ", "
      Add-Result "emscripten-wasm-lua54compat" "FAIL" "emcc probe no longer reports the known missing LuaJIT target backend for $targets; wire a real Emscripten artifact build and smoke before marking this target complete."
    } else {
      $targets = ($probes | ForEach-Object { $_.Target }) -join ", "
      Add-Result "emscripten-wasm-lua54compat" "SKIP" "$version; $targets probes reach LuaJIT target selection but fail because this tree has no wasm/wasm64 LuaJIT target or VM backend yet."
    }
  } else {
    Add-Result "emscripten-wasm-lua54compat" "SKIP" "emcc not found; Emscripten build path also needs interpreter/wasm support."
  }
}

function Add-PlatformProbes {
  Invoke-IosArm64
  Invoke-EmscriptenWasm
}

if ($Target -eq "all" -or $Target -eq "pc") {
  Invoke-PcX64
}

if ($Target -eq "all" -or $Target -eq "android") {
  Invoke-AndroidArm64
}

if ($Target -eq "ios") {
  Invoke-IosArm64
}

if ($Target -eq "emscripten") {
  Invoke-EmscriptenWasm
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
