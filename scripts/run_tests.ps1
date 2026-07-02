param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

function Invoke-Step {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    Write-Host "==> $Name"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

Invoke-Step "Configure CMake" {
    cmake -S . -B $BuildDir
}

Invoke-Step "Build C++ targets" {
    cmake --build $BuildDir
}

Invoke-Step "Run C++ regression tests" {
    ctest --test-dir $BuildDir --output-on-failure --timeout 30
}

Invoke-Step "Run Python scanner tests" {
    python tests\scripts\live_rsi_divergence_scan_tests.py
}

Invoke-Step "Run Python historical downloader tests" {
    python tests\scripts\download_historical_candles_tests.py
}
