# Pulls the PandaTouch tablet's data backup to a safe, OFF-DEVICE location.
#
# The tablet already keeps a rolling snapshot in /sdcard/ptbackup, but that lives
# on the very device it is meant to protect against. This script fetches the same
# data over the LAN (GET /backup) and writes a dated copy somewhere else - by
# default the NAS - so a dead/wiped tablet doesn't take the spool library and the
# whole cost history with it.
#
# Run it once by hand, or install it as a daily scheduled task:
#     .\backup_pull.ps1 -Install
#
# Notes:
#  - Give the tablet a fixed/reserved IP in your router, otherwise -TabletUrl
#    goes stale when DHCP hands it a new address.
#  - The backup contains NO secrets: the printer access code is deliberately
#    excluded from /backup (see sim/android/backup.h).

param(
    [string]$TabletUrl = "http://192.168.2.110:8080",
    [string]$Dest      = "X:\projects\panda\PandaTouch_backups",
    [int]   $Keep      = 30,      # how many dated copies to keep
    [switch]$Install              # register the daily scheduled task and exit
)

$ErrorActionPreference = 'Stop'
$taskName = 'PandaTouch backup pull'

if ($Install) {
    $script = $MyInvocation.MyCommand.Path
    $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
        -Argument ('-NoProfile -ExecutionPolicy Bypass -File "{0}" -TabletUrl "{1}" -Dest "{2}" -Keep {3}' -f $script, $TabletUrl, $Dest, $Keep)
    $trigger  = New-ScheduledTaskTrigger -Daily -At 20:00
    $settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -DontStopIfGoingOnBatteries -AllowStartIfOnBatteries
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    Write-Host "Scheduled task '$taskName' installed - runs daily at 20:00."
    Write-Host "Backups -> $Dest   (keeping the last $Keep)"
    exit 0
}

if (-not (Test-Path $Dest)) { New-Item -ItemType Directory -Path $Dest -Force | Out-Null }

$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
$file  = Join-Path $Dest "pandatouch-backup_$stamp.ptb"

try {
    $r = Invoke-WebRequest "$TabletUrl/backup" -UseBasicParsing -TimeoutSec 30
} catch {
    Write-Host "FAILED: tablet not reachable at $TabletUrl - $($_.Exception.Message)"
    exit 1
}

# Sanity-check the payload before we trust it: it must be our blob with sections,
# so a captive-portal/error page never overwrites a good backup rotation.
$text = [System.Text.Encoding]::UTF8.GetString($r.Content)
if ($text -notmatch '^#PTBACKUP1' -or $text -notmatch '@@spools') {
    Write-Host "FAILED: unexpected response (not a PandaTouch backup) - nothing written."
    exit 1
}

[System.IO.File]::WriteAllBytes($file, $r.Content)
Write-Host ("OK: {0} ({1:N0} bytes)" -f $file, $r.Content.Length)

# Keep only the newest $Keep copies.
$old = Get-ChildItem $Dest -Filter 'pandatouch-backup_*.ptb' | Sort-Object LastWriteTime -Descending | Select-Object -Skip $Keep
foreach ($f in $old) { Remove-Item $f.FullName -Force; Write-Host "  pruned $($f.Name)" }
