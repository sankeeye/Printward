# Pulls the FilaTrack tablet's data backup to a safe, OFF-DEVICE location.
#
# The tablet already keeps a rolling snapshot in /sdcard/filatrack_backup, but that lives
# on the very device it is meant to protect against. This script fetches the same
# data over the LAN (GET /backup) and writes a dated copy somewhere else, so a
# dead/wiped tablet doesn't take the spool library and the whole cost history
# with it.
#
#   .\backup_pull.ps1 -Install     -> pick a folder + time, install a daily task
#   .\backup_pull.ps1              -> pull once, right now
#   .\backup_pull.ps1 -Uninstall   -> remove the daily task again
#
# Notes:
#  - Give the tablet a fixed/reserved IP in your router, otherwise -TabletUrl
#    goes stale when DHCP hands it a new address.
#  - The backup contains NO secrets: the printer access code is deliberately
#    excluded from /backup (see sim/android/backup.h).

param(
    [string]$TabletUrl = "http://192.168.2.110:8080",
    [string]$Dest,                 # where to store backups; asked for on -Install
    [int]   $Keep      = 30,       # how many dated copies to keep
    [string]$At        = "20:00",  # daily run time, on -Install
    [string]$Pass      = "",       # web password (tablet: Instellingen > Webwachtwoord)
    [switch]$Install,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$taskName    = 'FilaTrack backup pull'
$defaultDest = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'FilaTrack_backups'

function Pick-Folder($startAt) {
    # Graphical folder picker; falls back to a typed path if there's no GUI.
    try {
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
        $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
        $dlg.Description         = 'Kies waar de FilaTrack back-ups moeten komen'
        $dlg.ShowNewFolderButton = $true
        if ($startAt -and (Test-Path $startAt)) { $dlg.SelectedPath = $startAt }
        if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return $dlg.SelectedPath }
        return $null
    } catch {
        $typed = Read-Host "Map voor de back-ups (Enter = $startAt)"
        if ([string]::IsNullOrWhiteSpace($typed)) { return $startAt }
        return $typed
    }
}

# --- uninstall ------------------------------------------------------------
if ($Uninstall) {
    if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
        Write-Host "Geplande taak '$taskName' verwijderd. Je back-ups blijven gewoon staan."
    } else {
        Write-Host "Er stond geen geplande taak '$taskName'."
    }
    exit 0
}

# --- install --------------------------------------------------------------
if ($Install) {
    if (-not $Dest) {
        $Dest = Pick-Folder $defaultDest
        if (-not $Dest) { Write-Host "Geannuleerd - niets geinstalleerd."; exit 1 }
    }
    if (-not (Test-Path $Dest)) { New-Item -ItemType Directory -Path $Dest -Force | Out-Null }

    # Sanity-check the time before handing it to the scheduler.
    try { $null = [datetime]::ParseExact($At, 'HH:mm', $null) }
    catch { Write-Host "Ongeldige tijd '$At' - gebruik bv. -At 20:00"; exit 1 }

    # Ask for the password now rather than let the nightly task fail silently at
    # 20:00 with nobody watching.
    if (-not $Pass) {
        Write-Host ""
        Write-Host "De tablet vraagt om een wachtwoord voor de webpagina."
        Write-Host "Je vindt het OP DE TABLET: Instellingen > Webwachtwoord."
        $Pass = Read-Host "Webwachtwoord"
    }
    if (-not $Pass) { Write-Host "Zonder wachtwoord kan de back-up niet opgehaald worden - geannuleerd."; exit 1 }

    $script = $MyInvocation.MyCommand.Path
    # NOTE: the password ends up in the scheduled task's arguments, readable by
    # anyone who can read your task list. It only guards a printer on your own LAN,
    # and the alternative (a second secret store) buys little here - but it is worth
    # knowing rather than discovering.
    $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
        -Argument ('-NoProfile -ExecutionPolicy Bypass -File "{0}" -TabletUrl "{1}" -Dest "{2}" -Keep {3} -Pass "{4}"' -f $script, $TabletUrl, $Dest, $Keep, $Pass)
    $trigger  = New-ScheduledTaskTrigger -Daily -At $At
    # StartWhenAvailable: catch up if the PC was off at the scheduled moment.
    $settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -DontStopIfGoingOnBatteries -AllowStartIfOnBatteries
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null

    Write-Host ""
    Write-Host "Geplande taak geinstalleerd." -ForegroundColor Green
    Write-Host "  map     : $Dest"
    Write-Host "  tijd    : elke dag om $At (haalt in als de pc uit stond)"
    Write-Host "  bewaart : de laatste $Keep back-ups"
    Write-Host "  tablet  : $TabletUrl"
    Write-Host ""
    Write-Host "Nu meteen een back-up maken? Draai:  .\backup_pull.ps1 -Dest `"$Dest`""
    Write-Host "Weer weghalen:                       .\backup_pull.ps1 -Uninstall"
    exit 0
}

# --- pull once ------------------------------------------------------------
if (-not $Dest) { $Dest = $defaultDest }
if (-not (Test-Path $Dest)) { New-Item -ItemType Directory -Path $Dest -Force | Out-Null }

$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
$file  = Join-Path $Dest "filatrack-backup_$stamp.ptb"

# The tablet needs a password now (Instellingen > Webwachtwoord on its screen).
$hdr = @{}
if ($Pass) { $hdr = @{ Authorization = "Basic " + [Convert]::ToBase64String(
                       [Text.Encoding]::ASCII.GetBytes("filatrack:$Pass")) } }
try {
    $r = Invoke-WebRequest "$TabletUrl/backup" -Headers $hdr -UseBasicParsing -TimeoutSec 30
} catch {
    if ($_.Exception.Response -and [int]$_.Exception.Response.StatusCode -eq 401) {
        Write-Host "MISLUKT: wachtwoord nodig of onjuist."
        Write-Host "  Kijk op de tablet: Instellingen > Webwachtwoord, en start dit met:"
        Write-Host "    .\backup_pull.ps1 -Pass <wachtwoord>"
        Write-Host "  Bij -Install wordt het wachtwoord in de geplande taak meegegeven."
        exit 1
    }
    Write-Host "MISLUKT: tablet niet bereikbaar op $TabletUrl - $($_.Exception.Message)"
    exit 1
}

# Sanity-check the payload before we trust it: it must be our blob with sections,
# so a captive-portal/error page never overwrites a good backup rotation.
$text = [System.Text.Encoding]::UTF8.GetString($r.Content)
if ($text -notmatch '^#PTBACKUP1' -or $text -notmatch '@@spools') {
    Write-Host "MISLUKT: onverwacht antwoord (geen FilaTrack back-up) - niets weggeschreven."
    exit 1
}

[System.IO.File]::WriteAllBytes($file, $r.Content)
Write-Host ("OK: {0} ({1:N0} bytes)" -f $file, $r.Content.Length)

# Keep only the newest $Keep copies.
$old = Get-ChildItem $Dest -Filter 'filatrack-backup_*.ptb' | Sort-Object LastWriteTime -Descending | Select-Object -Skip $Keep
foreach ($f in $old) { Remove-Item $f.FullName -Force; Write-Host "  opgeruimd: $($f.Name)" }
