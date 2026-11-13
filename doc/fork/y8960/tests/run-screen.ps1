param(
    [Parameter(Mandatory = $true)][string]$Exe,     # blueMSX+.exe of the tree under test
    [string]$Rom = "",                              # a test cartridge image; none when the machine has it
    [Parameter(Mandatory = $true)][string]$Shot,    # where the screenshot goes
    [string]$RomType = "Y8960",                     # the whole card carries every block
    [string]$Machine = "MSX2+ - C-BIOS + Y8960 (whole cart)",
    [string]$Root = (Join-Path ([IO.Path]::GetTempPath()) "bluemsx-screen"),
    [string[]]$Seconds = @("15")                    # when to shoot, from the start
)

# powershell -File hands "16,60" over as one string, which an [int[]]
# parameter reads as 1660, the comma taken for a thousands separator.
$Seconds = @($Seconds -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { [int]$_ })

# Runs a test cartridge in a hidden blueMSX+ and leaves a screenshot of what
# it printed at $Shot. Nothing is shown or heard. /rootdir keeps the settings
# and the screenshot away from the user's own.
#
# A test that prints more than a screen holds is shot more than once: given
# several times, each shot goes to $Shot with the time put before the
# extension (banktest-20s.png).

. (Join-Path $PSScriptRoot "..\..\automation\tests\BlueMsxControl.ps1")

$Exe = (Resolve-Path $Exe).Path
New-Item -ItemType Directory -Force $Root | Out-Null

$argv = @('/machine', "`"$Machine`"")
if ($Rom) {
    $Rom = (Resolve-Path $Rom).Path
    $argv += @('/rom1', "`"$Rom`"", '/romtype1', $RomType)
}
$argv += @('/rootdir', "`"$Root`"", '/hidden')
$p = Start-Process -FilePath $Exe -ArgumentList $argv -WorkingDirectory (Split-Path $Exe) -PassThru
$null = $p.Handle

try {
    $h = Find-BlueMsxWindow $p
    if ($h -eq [IntPtr]::Zero) { "no window; exit code $($p.ExitCode)"; exit 1 }

    $start = Get-Date
    foreach ($at in ($Seconds | Sort-Object)) {
        $wait = $at - ((Get-Date) - $start).TotalSeconds
        if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
        $before = Get-Date
        Invoke-BlueMsxMenu $h $BlueMsxMenu.Screenshot
        Start-Sleep -Seconds 2

        $img = Get-ChildItem -Path $Root, (Split-Path $Exe) -Recurse -Include *.png, *.bmp -ErrorAction SilentlyContinue |
               Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime -Descending |
               Select-Object -First 1
        if (-not $img) { "no screenshot written at $at s"; exit 1 }
        $to = $Shot
        if ($Seconds.Count -gt 1) {
            $to = Join-Path (Split-Path $Shot) ("{0}-{1}s{2}" -f [IO.Path]::GetFileNameWithoutExtension($Shot), $at, [IO.Path]::GetExtension($Shot))
        }
        Move-Item -Force $img.FullName $to
        "screenshot $to"
    }
    "visible windows: $([BlueMsxControl]::VisibleWindows([uint32]$p.Id))"
}
finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
