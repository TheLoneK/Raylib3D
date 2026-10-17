# Only affects this one terminal session; resets when it closes
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# Or for just one script, without changing anything
powershell -ExecutionPolicy Bypass -File .\run.ps1