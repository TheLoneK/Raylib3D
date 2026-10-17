# Build + run (Windows, MinGW gcc). Keeps raylib local: headers in include/, library in lib/
# Every .c file in src/ is compiled, so new modules are picked up automatically.

$sources = @("main.c") + (Get-ChildItem -Path "src" -Filter "*.c" | ForEach-Object { "src/" + $_.Name })

gcc @sources -o TestGame.exe -O1 -Wall -std=c99 -Wno-missing-braces -I include/ -I src/ -L lib/ -lraylib -lopengl32 -lgdi32 -lwinmm

if ($LASTEXITCODE -eq 0) { ./TestGame.exe }
