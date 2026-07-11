# Syncs the current repo sources into an ndk-build jni/ tree, so the Android app
# builds the SAME UI code as the device firmware + PC simulator.
#
# Usage:  .\sync_sources.ps1 -Jni C:\pt_android\app\jni
#
# The jni/ tree must already contain the third-party bits that are NOT in git
# (see README.md): SDL/, lvgl/, mbedtls/ source trees. This script only copies
# OUR sources + the two header/PubSubClient libs pulled from .pio/libdeps.

param(
    [Parameter(Mandatory = $true)] [string]$Jni
)

$repo = Split-Path -Parent $PSScriptRoot   # ...\PandaTouch_streamDeck_src
$src  = Join-Path $Jni "src"
$flags = "/NFL /NDL /NJH /NJS /NP".Split(" ")

Write-Host "repo = $repo"
Write-Host "jni  = $Jni"

# 1. real firmware sources -> ourui/src
robocopy "$repo\src" "$src\ourui\src" /E @flags | Out-Null
# 2. Arduino/hardware shims -> ourui/shim
robocopy "$repo\sim\shim" "$src\ourui\shim" /E @flags | Out-Null
# 3. entry point + Android glue (storage globals + config loader + FTP stub)
robocopy "$repo\sim"         "$src\ourui" main.cpp @flags | Out-Null
robocopy "$repo\sim\android" "$src\ourui" android_glue.cpp @flags | Out-Null
# 3b. Android-only screens (printer setup + idle screensaver) -> next to ui_*
robocopy "$repo\sim\android" "$src\ourui\src" ui_tablet_setup.cpp ui_tablet_setup.h ui_screensaver.cpp ui_screensaver.h gcode_view.cpp gcode_view.h webctl.cpp webctl.h @flags | Out-Null
# 4. PubSubClient (compiled) + ArduinoJson (header-only) from PlatformIO libdeps
robocopy "$repo\.pio\libdeps\pandatouch\PubSubClient\src" "$src\pubsubclient" /E @flags | Out-Null
robocopy "$repo\.pio\libdeps\pandatouch\ArduinoJson\src"  "$src\arduinojson"  /E @flags | Out-Null

Write-Host "Sync done. Now: gradle assembleDebug"
