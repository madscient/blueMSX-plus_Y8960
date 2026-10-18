param(
    [Parameter(Mandatory = $true)][string]$Exe,     # blueMSX+.exe of the tree under test
    [string]$Machine = "MSX2+ - C-BIOS",
    [string]$Root = (Join-Path ([IO.Path]::GetTempPath()) "bluemsx-mixer-settings"),
    [int]$RunMs = 3000
)

# Starts blueMSX+ hidden on a settings file seeded with mixer instance lines,
# closes it, and checks what it wrote back. Only a load and a save that both
# understood the lines can change them the way the checks expect: a volume
# out of range comes back clamped, and an instance given only some of its
# attributes comes back with the rest at their defaults. Lines the program
# does not understand are left as they were.
#
# /rootdir keeps the settings away from the user's own.

. (Join-Path $PSScriptRoot "..\..\automation\tests\BlueMsxControl.ps1")

$Exe = (Resolve-Path $Exe).Path
if (Test-Path $Root) { Remove-Item -Recurse -Force $Root }
New-Item -ItemType Directory -Force $Root | Out-Null
$ini = Join-Path $Root "bluemsx.ini"

$seed = @(
    "[config]",
    "sound.mix.scc.slot1-0.volume=150",
    "sound.mix.scc.slot1-0.pan=30",
    "sound.mix.psg.board.enable=no",
    "sound.mix.bad=7"
)
Set-Content -Path $ini -Value $seed -Encoding ascii

$args2 = @('/machine', "`"$Machine`"", '/rootdir', "`"$Root`"", '/hidden')
$p = Start-Process -FilePath $Exe -ArgumentList $args2 -WorkingDirectory (Split-Path $Exe) -PassThru
$null = $p.Handle

# Closed in finally, so that a run cut short does not leave the hidden instance running.
try {
    $h = Find-BlueMsxWindow $p
    if ($h -eq [IntPtr]::Zero) { "no window; exit code $($p.ExitCode)"; exit 1 }
    Start-Sleep -Milliseconds $RunMs

    [void][BlueMsxControl]::SendMsg($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_CLOSE
    $exited = $p.WaitForExit(15000) -and $p.ExitCode -eq 0
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
finally {
    if (-not $p.HasExited) {
        $w = Find-BlueMsxWindow $p 2000
        if ($w -ne [IntPtr]::Zero) { [void][BlueMsxControl]::SendMsg($w, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
        if (-not $p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force }
    }
}

$lines = Get-Content $ini
$mix = $lines | Where-Object { $_ -like "sound.mix.*" }

$results = [ordered]@{
    "exits on WM_CLOSE"                    = $exited
    "volume out of range comes back 100"   = $lines -contains "sound.mix.scc.slot1-0.volume=100"
    "pan comes back as it was"             = $lines -contains "sound.mix.scc.slot1-0.pan=30"
    "missing enable is written as yes"     = $lines -contains "sound.mix.scc.slot1-0.enable=yes"
    "enable=no comes back"                 = $lines -contains "sound.mix.psg.board.enable=no"
    "missing volume is written as 100"     = $lines -contains "sound.mix.psg.board.volume=100"
    "missing pan is written as 50"         = $lines -contains "sound.mix.psg.board.pan=50"
    "a line without an id is left alone"   = $lines -contains "sound.mix.bad=7"
    "no other mix lines are written"       = @($mix).Count -eq 7
}

$results.GetEnumerator() | ForEach-Object { "{0,-38} {1}" -f $_.Key, $(if ($_.Value) { "OK" } else { "NG" }) }
"--- sound.mix lines written back"
$mix
if ($results.Values -contains $false) { exit 1 }
