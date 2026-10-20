param(
    [Parameter(Mandatory = $true)][string]$Exe,     # blueMSX+.exe of the tree under test
    [Parameter(Mandatory = $true)][string]$Rom,     # built by make-sndtest.py
    [Parameter(Mandatory = $true)][string]$Wav,     # where the recording goes
    [string]$Machine = "MSX2+ - C-BIOS + Y8960 (cartridge)",
    [string]$Root = (Join-Path ([IO.Path]::GetTempPath()) "bluemsx-sndtest"),
    [int]$Seconds = 30                              # boot plus the 17 steps
)

# Runs sndtest.rom in a hidden blueMSX+ with the WAV capture on, and leaves
# the recording at $Wav for analyze-sndtest.py. Nothing is shown or heard.
#
# The capture starts as soon as the window exists, before the cartridge's
# init runs, so the first step is not cut off. /rootdir keeps the settings
# and the capture away from the user's own.

. (Join-Path $PSScriptRoot "..\..\automation\tests\BlueMsxControl.ps1")

$Exe = (Resolve-Path $Exe).Path
$Rom = (Resolve-Path $Rom).Path
New-Item -ItemType Directory -Force $Root | Out-Null

$argv = @('/machine', "`"$Machine`"", '/rom1', "`"$Rom`"", '/romtype1', 'Y8960SCC',
          '/rootdir', "`"$Root`"", '/hidden')
$p = Start-Process -FilePath $Exe -ArgumentList $argv -WorkingDirectory (Split-Path $Exe) -PassThru
$null = $p.Handle

try {
    $h = Find-BlueMsxWindow $p
    if ($h -eq [IntPtr]::Zero) { "no window; exit code $($p.ExitCode)"; exit 1 }

    $before = Get-Date
    Invoke-BlueMsxMenu $h $BlueMsxMenu.LogWav
    Start-Sleep -Seconds $Seconds
    Invoke-BlueMsxMenu $h $BlueMsxMenu.LogWav
    Start-Sleep -Seconds 1

    $rec = Get-ChildItem -Path $Root, (Split-Path $Exe) -Recurse -Filter *.wav -ErrorAction SilentlyContinue |
           Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime -Descending |
           Select-Object -First 1
    if (-not $rec) { "no recording written"; exit 1 }
    Move-Item -Force $rec.FullName $Wav
    "recorded $Wav"
}
finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
