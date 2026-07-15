# Regression self-test for the FilaTrack tablet, over its LAN HTTP API.
#
# Why an integration test and not unit tests: the data modules (history, backup,
# spool_db, filament_track) read/write fixed /sdcard paths, so unit-testing them
# on the host would need a path-injection refactor. Driving the real device costs
# nothing extra and catches the regressions we actually hit:
#   - the roll price being wiped when a roll was re-weighed
#   - /backup losing a section, or leaking the printer access code
#   - history/spools JSON losing fields the web depends on
#
# SAFE BY DESIGN: it only ever writes one throwaway roll (ZZ_SELFTEST) and always
# deletes it again, matching on the name before removing anything. It never
# touches your real rolls, and never exercises /restore (that would overwrite
# live data).
#
#     .\selftest.ps1                       # against the default tablet
#     .\selftest.ps1 -TabletUrl http://...

param([string]$TabletUrl = "http://192.168.2.110:8080")

$ErrorActionPreference = 'Stop'
$script:fails = 0
$TESTNAME = 'ZZ_SELFTEST'

function Check($what, $ok, $detail = '') {
    if ($ok) { Write-Host ("  PASS  {0}" -f $what) -ForegroundColor Green }
    else     { Write-Host ("  FAIL  {0} {1}" -f $what, $detail) -ForegroundColor Red; $script:fails++ }
}
function Get-Json($p)  { Invoke-RestMethod "$TabletUrl$p" -TimeoutSec 15 }
function Hit($p)       { Invoke-WebRequest "$TabletUrl$p" -UseBasicParsing -TimeoutSec 20 }
function Find-Test     { (Get-Json '/spools') | Where-Object { $_.name -eq $TESTNAME } | Select-Object -First 1 }

Write-Host "FilaTrack self-test -> $TabletUrl`n"

# --- 1. status ------------------------------------------------------------
Write-Host "status:"
try {
    $s = Get-Json '/status'
    $need = @('conn','state','pct','printg','printcost','file','active_tray','cfg')
    $miss = $need | Where-Object { $s.PSObject.Properties.Name -notcontains $_ }
    Check "velden aanwezig" ($miss.Count -eq 0) ("mist: " + ($miss -join ','))
    Check "cfg.low is een getal" ($s.cfg.low -ge 0)
} catch { Check "bereikbaar" $false $_.Exception.Message }

# --- 2. diag --------------------------------------------------------------
Write-Host "diag:"
try {
    $d = Get-Json '/diag'
    Check "endpoint antwoordt" ($null -ne $d.uptime)
    $bad = @($d.files | Where-Object { -not $_.ok })
    Check "alle databestanden aanwezig" ($bad.Count -eq 0) ("ontbreekt: " + (($bad | ForEach-Object { $_.n }) -join ','))
} catch { Check "bereikbaar" $false $_.Exception.Message }

# --- 3. backup ------------------------------------------------------------
Write-Host "backup:"
try {
    $r = Hit '/backup'
    $txt = [System.Text.Encoding]::UTF8.GetString($r.Content)
    Check "magic header" ($txt -match '^#PTBACKUP1')
    foreach ($sec in @('spools','empties','weights','history','stats')) {
        Check "sectie @@$sec" ($txt -match ("(?m)^@@" + $sec + "$"))
    }
    Check "GEEN toegangscode/secret in de back-up" (-not ($txt -match 'access|serial=|code='))
    Check "download-header" ($r.Headers['Content-Disposition'] -match 'filatrack-backup')
} catch { Check "bereikbaar" $false $_.Exception.Message }

# --- 4. history / spools JSON contract ------------------------------------
Write-Host "history + spools JSON:"
try {
    # NOTE: Invoke-RestMethod hands back the whole JSON array as one Object[],
    # so do NOT wrap it in @() - that nests it and $h[0] becomes the array itself.
    $h = Get-Json '/history'
    if ($h -and $h.Count -gt 0) {
        $names = $h[0].PSObject.Properties.Name
        $need = @('i','when','name','grams','cost','ok','file','arch','mat','ts','mins')
        $miss = $need | Where-Object { $names -notcontains $_ }
        Check "history velden" ($miss.Count -eq 0) ("mist: " + ($miss -join ','))
    } else { Write-Host "  SKIP  history leeg" -ForegroundColor DarkGray }
    $sp = Get-Json '/spools'
    if ($sp -and $sp.Count -gt 0) {
        $names = $sp[0].PSObject.Properties.Name
        $need = @('i','name','material','rgb','rem','live','slot','empty','price')
        $miss = $need | Where-Object { $names -notcontains $_ }
        Check "spools velden" ($miss.Count -eq 0) ("mist: " + ($miss -join ','))
    } else { Write-Host "  SKIP  geen rollen" -ForegroundColor DarkGray }
} catch { Check "bereikbaar" $false $_.Exception.Message }

# --- 5. price survives a save that omits it (the weigh regression) ---------
Write-Host "prijs blijft behouden bij wegen:"
try {
    if (Find-Test) { Write-Host "  (oude testrol gevonden, wordt opgeruimd)" -ForegroundColor DarkGray }
    else {
        Hit ("/spool_save?idx=&name=$TESTNAME&material=PLA&color=%23888888&rem=500&empty=200&price=9.99") | Out-Null
        Start-Sleep -Milliseconds 600   # /spool_save wacht niet op de hoofdthread (anders dan /setcfg)
    }
    $t = Find-Test
    Check "testrol aangemaakt" ($null -ne $t)
    if ($t) {
        Check "prijs opgeslagen (9.99)" ([math]::Abs($t.price - 9.99) -lt 0.01) ("kreeg " + $t.price)
        # simulate the weigh path: same roll, new grams, NO price param
        Hit ("/spool_save?idx=" + $t.i + "&name=$TESTNAME&material=PLA&color=%23888888&rem=480&empty=200") | Out-Null
        Start-Sleep -Milliseconds 600
        $t2 = Find-Test
        Check "prijs behouden na save zonder price" ($null -ne $t2 -and [math]::Abs($t2.price - 9.99) -lt 0.01) ("kreeg " + $t2.price)
        Check "gewicht wel bijgewerkt (480)" ($null -ne $t2 -and [math]::Abs($t2.rem - 480) -lt 1) ("kreeg " + $t2.rem)
    }
} catch { Check "prijs-test" $false $_.Exception.Message }
finally {
    # Always clean up - and only ever delete a roll whose name matches exactly.
    $t = Find-Test
    if ($t -and $t.name -eq $TESTNAME) {
        Hit ("/spool_del?idx=" + $t.i) | Out-Null
        Start-Sleep -Milliseconds 500
        Check "testrol opgeruimd" ($null -eq (Find-Test))
    }
}

# --- taal: elke taal moet compleet zijn -----------------------------------
# Waarom: "op de site is niet alles Engels" kwam twee keer terug. Een taal is pas
# af als hij dezelfde sleutels heeft als Engels; ontbreekt er een, dan valt die
# stilletjes terug op Engels en zie je dat pas op het scherm.
#
# Zet de taal tijdelijk om en zet hem daarna terug - net als de wegwerprol.
Write-Host "taal:"
$origLang = $null
try {
    $origLang = (Get-Json '/status').cfg.lang
    $codes = Get-Json '/langs'
    Check "meer dan 1 taal beschikbaar" ($codes.Count -ge 2) "-> $($codes -join ',')"

    # GEEN sleep tussen setcfg en /lang. Dat is precies de bug die Arno meldde:
    # /setcfg zette de wijziging in de wachtrij en antwoordde meteen, dus /lang gaf
    # nog de oude tabel en je moest een paar keer klikken. /setcfg wacht nu tot de
    # hoofdthread het heeft toegepast; een sleep hier zou dat weer verstoppen.
    $keysPer = @{}
    foreach ($c in $codes) {
        Hit "/setcfg?lang=$c" | Out-Null
        $l = Get-Json '/lang'
        Check "/lang volgt taal $c meteen (geen race)" ($l.lang -eq $c) "-> kreeg '$($l.lang)'"
        $keysPer[$c] = @($l.s.PSObject.Properties.Name)
    }

    $en = $keysPer['en']
    Check "Engels heeft sleutels" ($en.Count -gt 100) "-> $($en.Count)"
    foreach ($c in $codes) {
        if ($c -eq 'en') { continue }
        $missing = $en | Where-Object { $keysPer[$c] -notcontains $_ }
        Check "$c is compleet t.o.v. Engels" ($missing.Count -eq 0) `
              "-> mist $($missing.Count): $(($missing | Select-Object -First 6) -join ', ')"
    }

    # Een lege vertaling toont een leeg label op het scherm.
    foreach ($c in $codes) {
        Hit "/setcfg?lang=$c" | Out-Null
        $l = Get-Json '/lang'
        $blank = @($l.s.PSObject.Properties | Where-Object { -not $_.Value })
        Check "$c heeft geen lege teksten" ($blank.Count -eq 0) `
              "-> leeg: $(($blank.Name | Select-Object -First 6) -join ', ')"
    }
} catch {
    Check "taalcontrole" $false $_.Exception.Message
} finally {
    if ($origLang) { try { Hit "/setcfg?lang=$origLang" | Out-Null } catch {} }
}

Write-Host ""
if ($script:fails -eq 0) { Write-Host "ALLES GOED" -ForegroundColor Green; exit 0 }
Write-Host ("$script:fails CHECK(S) GEFAALD") -ForegroundColor Red
exit 1
