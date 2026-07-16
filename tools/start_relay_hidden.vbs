' Bambu Cloud Relay - Printward (verborgen starter)
' =====================================================
' Start start_relay.bat volledig onzichtbaar: geen console-venster.
' De relay blijft op de achtergrond draaien (met dezelfde auto-herstart
' als de .bat). Om te stoppen: beeindig "python.exe" via Taakbeheer.
'
' Let op: omdat er geen venster is, kun je hier geen 6-cijferige code
' invoeren als Bambu daar (ongeveer elke ~90 dagen) om vraagt. Gebeurt
' dat, start dan eenmalig start_relay.bat gewoon dubbelklikkend om in te
' loggen; daarna kan de verborgen starter het weer overnemen.

Set fso = CreateObject("Scripting.FileSystemObject")
scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)

Set shell = CreateObject("WScript.Shell")
shell.CurrentDirectory = scriptDir
' 0 = venster verborgen, False = niet wachten tot het klaar is
shell.Run "cmd /c """ & scriptDir & "\start_relay.bat""", 0, False
