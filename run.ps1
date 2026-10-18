# Build + run (Windows). Needs CMake and MinGW gcc/g++ on PATH.
#
#   ./run.ps1            build (only what changed) and run
#   ./run.ps1 -Clean     delete the build folder first (use after changing compilers or CMakeLists.txt options)
#   ./run.ps1 -NoRun     build only
#   ./run.ps1 -Debug     debug build (slower game, better for a debugger)
#   ./run.ps1 -Editor    build, then run the asset editor instead of the game
#
# The first build compiles flecs and takes a while; later builds only recompile changed files.

param(
    [switch]$Clean,
    [switch]$NoRun,
    [switch]$Debug,
    [switch]$Editor
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$buildType = if ($Debug) { "Debug" } else { "Release" }
$buildDir = "build/$buildType"

if ($Clean -and (Test-Path "build")) { Remove-Item -Recurse -Force "build" }

# Configure once (or again if the build folder was deleted)
if (-not (Test-Path "$buildDir/CMakeCache.txt")) {
    $generator = if (Get-Command ninja -ErrorAction SilentlyContinue) { "Ninja" } else { "MinGW Makefiles" }
    $configureArgs = @("-S", ".", "-B", $buildDir, "-G", $generator, "-DCMAKE_BUILD_TYPE=$buildType",
                       "-DCMAKE_C_COMPILER=gcc", "-DCMAKE_CXX_COMPILER=g++")

    # MinGW Makefiles needs a make program; some MinGW installs only ship "make" instead of "mingw32-make"
    if ($generator -eq "MinGW Makefiles" -and -not (Get-Command mingw32-make -ErrorAction SilentlyContinue)) {
        $make = Get-Command make -ErrorAction SilentlyContinue
        if ($make) { $configureArgs += "-DCMAKE_MAKE_PROGRAM=$($make.Source)" }
    }

    cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed" -ForegroundColor Red; exit $LASTEXITCODE }
}

cmake --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed" -ForegroundColor Red; exit $LASTEXITCODE }

if (-not $NoRun) {
    if ($Editor) { ./AssetEditor.exe } else { ./TestGame.exe }
}
