param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Rom,
    [Parameter(Mandatory = $true)][string]$Root,
    [string]$Machine = "MSX2+ - Sony HB-F1XDJ+",
    [int]$Seconds = 22,
    [string]$Special = "MAKOTO",
    [string]$Install = ""
)
# Runs opnatest.rom in slot 1 with Makoto in slot 2, hidden, records the
# mixer to a WAV and prints its path. /rootdir keeps the settings and the
# recording out of the install the user runs. The rhythm ROM is read from
# Machines/Shared Roms beside the exe.
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $here "..\..\automation\tests\BlueMsxControl.ps1")

New-Item -ItemType Directory -Force $Root | Out-Null
$capture = Join-Path $Root "Audio Capture"
$before = @()
if (Test-Path $capture) { $before = Get-ChildItem $capture -Filter *.wav | ForEach-Object { $_.FullName } }

$argv = @('/machine', "`"$Machine`"", '/rom1', "`"$Rom`"",
          '/rootdir', "`"$Root`"", '/hidden')
if ($Special -ne "") { $argv += @('/special2', $Special) }
# An exe built elsewhere runs from an install: machines from its Machines,
# and the rhythm ROM through its working directory.
if ($Install -ne "") { $argv += @('/machinedir', "`"$(Join-Path $Install 'Machines')`"") }
$work = if ($Install -ne "") { $Install } else { Split-Path $Exe }
$p = Start-Process -FilePath $Exe -ArgumentList $argv -WorkingDirectory $work -PassThru
try {
    $w = Find-BlueMsxWindow $p 15000
    if ($w -eq [IntPtr]::Zero) { throw "no window (exited: $($p.HasExited))" }
    Invoke-BlueMsxMenu $w 40016
    Start-Sleep -Seconds $Seconds
    Invoke-BlueMsxMenu $w 40016
    Start-Sleep -Seconds 2
}
finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
}
$after = Get-ChildItem $capture -Filter *.wav | Where-Object { $before -notcontains $_.FullName } |
    Sort-Object LastWriteTime | Select-Object -Last 1
if ($null -eq $after) { throw "no recording in $capture" }
$after.FullName
