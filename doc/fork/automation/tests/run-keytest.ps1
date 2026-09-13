param(
    [Parameter(Mandatory = $true)][string]$Rom,     # built by make-keytest.py
    [string]$Exe = (Join-Path $PSScriptRoot "..\..\..\..\blueMSX\Make\msvc2022\x64\Release\blueMSX+.exe"),
    [string]$Machine = "MSX2+ - C-BIOS",
    [string]$Root = (Join-Path ([IO.Path]::GetTempPath()) "bluemsx-keytest"),
    [int]$BootMs = 8000,
    [switch]$Visible
)

# Types "HELLO, MSX 123!" into keytest.rom through the key matrix and checks
# the parts of the interface that need no reading of the screen. The line
# KEYS: OK in the screenshot is the verdict on the typing itself.
#
# /rootdir keeps the settings and the screenshot away from the user's own.

. (Join-Path $PSScriptRoot "BlueMsxControl.ps1")

$Exe = (Resolve-Path $Exe).Path
$Rom = (Resolve-Path $Rom).Path
New-Item -ItemType Directory -Force $Root | Out-Null

$args2 = @('/machine', "`"$Machine`"", '/rom1', "`"$Rom`"", '/rootdir', "`"$Root`"")
if (-not $Visible) { $args2 += '/hidden' }
$p = Start-Process -FilePath $Exe -ArgumentList $args2 -WorkingDirectory (Split-Path $Exe) -PassThru
$null = $p.Handle

$h = Find-BlueMsxWindow $p
if ($h -eq [IntPtr]::Zero) { "no window; exit code $($p.ExitCode)"; exit 1 }

# Boot is waited out in emulated time, which lasts the same on any host. The
# request is refused until the machine is known, so it is retried.
$t0 = Get-Date
while ((Send-KeyMatrix $h "w $BootMs") -eq 0 -and ((Get-Date) - $t0).TotalSeconds -lt 20) { Start-Sleep -Milliseconds 100 }
[void](Wait-KeyMatrix $h)

$results = [ordered]@{
    "refuses row 12"          = (Send-KeyMatrix $h 'd 12 0x01') -eq 0
    "refuses a bit with no key" = (Send-KeyMatrix $h 'd 11 0x01') -eq 0
    "refuses unknown command" = (Send-KeyMatrix $h 'x 1 1') -eq 0
    "refuses missing number"  = (Send-KeyMatrix $h 'd 2 0x40 w') -eq 0
    "refuses a sign"          = (Send-KeyMatrix $h 'w -5') -eq 0
    "accepts the typing"      = (Send-KeyMatrix $h (ConvertTo-KeyMatrix "HELLO, MSX 123!`n")) -eq 1
}
$results["queue drains"] = Wait-KeyMatrix $h
Start-Sleep -Seconds 1

if (-not $Visible) {
    $results["no visible window"] = [BlueMsxControl]::VisibleWindows([uint32]$p.Id) -eq 0
    $results["refuses a handed-over file"] = [BlueMsxControl]::CopyData($h, $BlueMsxLaunchId, $Rom) -eq 0
}

$before = Get-Date
Invoke-BlueMsxMenu $h $BlueMsxMenu.Screenshot
Start-Sleep -Seconds 2
$shot = Get-ChildItem -Path $Root -Recurse -Include *.png, *.bmp -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime -Descending | Select-Object -First 1

[void][BlueMsxControl]::SendMsg($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)   # WM_CLOSE
$results["exits on WM_CLOSE"] = $p.WaitForExit(15000) -and $p.ExitCode -eq 0
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }

$results.GetEnumerator() | ForEach-Object { "{0,-28} {1}" -f $_.Key, $(if ($_.Value) { "OK" } else { "NG" }) }
"screenshot                   $(if ($shot) { $shot.FullName } else { 'none' })"
