$ErrorActionPreference = "Stop"

$Root = "D:\ethercat_master_windows"
$Soem = Join-Path $Root "SOEM"
$Generated = Join-Path $Root "generated"
$Out = Join-Path $PSScriptRoot "soem_pdo_control.exe"
$Source = Join-Path $PSScriptRoot "soem_pdo_control.cpp"
$Build = Join-Path $PSScriptRoot "build_soem_pdo_control"

if (-not (Test-Path $Soem)) {
    throw "SOEM source not found: $Soem"
}

$commonSources = @(
    "$Soem\src\ec_base.c",
    "$Soem\src\ec_coe.c",
    "$Soem\src\ec_config.c",
    "$Soem\src\ec_dc.c",
    "$Soem\src\ec_eoe.c",
    "$Soem\src\ec_foe.c",
    "$Soem\src\ec_main.c",
    "$Soem\src\ec_print.c",
    "$Soem\src\ec_soe.c",
    "$Soem\osal\win32\osal.c",
    "$Soem\oshw\win32\oshw.c",
    "$Soem\oshw\win32\nicdrv.c"
)

$includeArgs = @(
    "-I$Generated\include",
    "-I$Soem\include",
    "-I$Soem\osal",
    "-I$Soem\osal\win32",
    "-I$Soem\oshw\win32",
    "-I$Soem\oshw\win32\wpcap\Include"
)

$wpcapLib = "$Soem\oshw\win32\wpcap\Lib"
if ([Environment]::Is64BitProcess -and (Test-Path "$wpcapLib\x64")) {
    $wpcapLib = "$wpcapLib\x64"
}

$libArgs = @(
    "-L$wpcapLib",
    "-lwpcap",
    "-lPacket",
    "-lws2_32",
    "-lwinmm"
)

New-Item -ItemType Directory -Force -Path $Build | Out-Null

$objects = @()
foreach ($src in $commonSources) {
    $obj = Join-Path $Build ((Split-Path $src -Leaf) + ".o")
    & gcc -std=c11 -D_UCRT -D_CRT_SECURE_NO_WARNINGS -Wall -Wextra -Wno-unused-parameter `
        @includeArgs -c $src -o $obj
    if ($LASTEXITCODE -ne 0) {
        throw "gcc failed while compiling $src"
    }
    $objects += $obj
}

$cppObj = Join-Path $Build "soem_pdo_control.cpp.o"
& g++ -std=c++17 -D_UCRT -D_CRT_SECURE_NO_WARNINGS -Wall -Wextra -Wno-unused-parameter `
    @includeArgs -c $Source -o $cppObj
if ($LASTEXITCODE -ne 0) {
    throw "g++ failed while compiling $Source"
}
$objects += $cppObj

& g++ @objects @libArgs -o $Out

if ($LASTEXITCODE -ne 0) {
    throw "g++ failed while linking"
}

Write-Host "Built: $Out"
