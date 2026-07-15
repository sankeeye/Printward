# Builds the FilaTrack UI simulator and launches it.
# Double-click run_sim.bat (which calls this) - no terminal typing needed.
$ErrorActionPreference = 'Stop'
$sim  = $PSScriptRoot
$bash = 'C:\msys64\usr\bin\bash.exe'

if (-not (Test-Path $bash)) {
    Write-Host "MSYS2 not found at $bash" -ForegroundColor Red
    Read-Host "Press Enter to close"; exit 1
}

# Convert "I:\Mijn Drive\...\sim" -> MSYS path "/i/Mijn Drive/.../sim"
$msys = '/' + $sim.Substring(0, 1).ToLower() + $sim.Substring(2).Replace('\', '/')

Write-Host "Building FilaTrack simulator..." -ForegroundColor Cyan
& $bash "$msys/build.sh"
if ($LASTEXITCODE -ne 0) {
    Write-Host "`nBUILD FAILED - see the errors above." -ForegroundColor Red
    Read-Host "Press Enter to close"; exit 1
}

Write-Host "Launching..." -ForegroundColor Green
Get-Process filatrack_sim -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300
Start-Process -FilePath (Join-Path $sim 'filatrack_sim.exe') -WorkingDirectory $sim
