param(
    [Parameter(Mandatory = $true)][string]$Exe,       # blueMSX+.exe of the tree under test
    [Parameter(Mandatory = $true)][string]$Machine,   # one that boots MSX-DOS, with a disk drive
    [Parameter(Mandatory = $true)][string]$Rom,       # what goes into the whole card; needs "AB" at 4000h
    [Parameter(Mandatory = $true)][string]$DiskDir,   # folder inserted as drive A, MSX-DOS 1 format
    [Parameter(Mandatory = $true)][string]$Out,       # prefix of the recording and the screenshots
    [string]$Root = (Join-Path ([IO.Path]::GetTempPath()) "bluemsx-dos"),
    [int]$Seconds = 90,                               # how long to record, from the start
    [int[]]$Shots = @(30)                             # when to shoot, from the start
)

# Boots MSX-DOS from a folder in a hidden blueMSX+ with the whole Y8960 card
# inserted, records everything to $Out.wav and shoots the screen at $Shots.
# Nothing is shown or heard. The folder needs MSXDOS.SYS and COMMAND.COM, and
# an AUTOEXEC.BAT naming the program runs it without any key press.
#
# Settings go in $Root\bluemsx.ini, first line [config]. A line such as
#   sound.mix.y8960.slot1-0.opl2.enable=no
# mutes one block of the card, the way the per-device mixer does.

. (Join-Path $PSScriptRoot "..\..\automation\tests\BlueMsxControl.ps1")

$Exe = (Resolve-Path $Exe).Path
$Rom = (Resolve-Path $Rom).Path
$DiskDir = (Resolve-Path $DiskDir).Path
New-Item -ItemType Directory -Force $Root | Out-Null

$argv = @('/machine', "`"$Machine`"", '/rom1', "`"$Rom`"", '/romtype1', 'Y8960',
          '/diskA', "`"$DiskDir`"", '/diskAformat', 'msxdos1',
          '/rootdir', "`"$Root`"", '/hidden')
$p = Start-Process -FilePath $Exe -ArgumentList $argv -WorkingDirectory (Split-Path $Exe) -PassThru
$null = $p.Handle

try {
    $h = Find-BlueMsxWindow $p
    if ($h -eq [IntPtr]::Zero) { "no window; exit code $($p.ExitCode)"; exit 1 }

    $start = Get-Date
    Invoke-BlueMsxMenu $h $BlueMsxMenu.LogWav
    foreach ($at in ($Shots | Sort-Object)) {
        $wait = $at - ((Get-Date) - $start).TotalSeconds
        if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
        $before = Get-Date
        Invoke-BlueMsxMenu $h $BlueMsxMenu.Screenshot
        Start-Sleep -Seconds 1
        $img = Get-ChildItem -Path $Root, (Split-Path $Exe) -Recurse -Include *.png, *.bmp -ErrorAction SilentlyContinue |
               Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime -Descending |
               Select-Object -First 1
        if ($img) { Move-Item -Force $img.FullName "$Out-$($at)s.png"; "screenshot $Out-$($at)s.png" }
        else { "no screenshot written at $at s" }
    }
    $wait = $Seconds - ((Get-Date) - $start).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    Invoke-BlueMsxMenu $h $BlueMsxMenu.LogWav
    Start-Sleep -Seconds 1

    $rec = Get-ChildItem -Path $Root, (Split-Path $Exe) -Recurse -Filter *.wav -ErrorAction SilentlyContinue |
           Where-Object { $_.LastWriteTime -gt $start } | Sort-Object LastWriteTime -Descending |
           Select-Object -First 1
    if (-not $rec) { "no recording written"; exit 1 }
    Move-Item -Force $rec.FullName "$Out.wav"
    "recorded $Out.wav"
    "visible windows: $([BlueMsxControl]::VisibleWindows([uint32]$p.Id))"
}
finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
