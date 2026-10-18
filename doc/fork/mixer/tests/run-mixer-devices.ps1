param(
    [Parameter(Mandatory = $true)][string]$Exe,     # blueMSX+.exe of the tree under test
    [string]$Machine = "MSX2+ - C-BIOS",
    [string]$Special1 = "SCC",
    [string]$Special2 = "SCC",
    [string]$Root = (Join-Path ([IO.Path]::GetTempPath()) "bluemsx-mixer-devices"),
    [int]$BootMs = 3000,
    [int]$SetVolume = 40,                            # given to the first row of $Group
    [string]$Group = "SCC",
    [switch]$Keep,                                   # keep the settings from a previous run
    [string]$Screenshot,                             # PNG of the devices window, if given
    [string[]]$ExtraArgs = @()                       # more command line, e.g. /rom1 and /romtype1
)

# Opens Tools > Sound Mixer (Devices) in a visible blueMSX+, prints the rows
# the window lists, moves the volume of the first row of $Group, and closes.
# Run it twice with -Keep the second time to see the saved value come back.
#
# Windows appear on the screen while it runs. /rootdir keeps the settings
# away from the user's own.

. (Join-Path $PSScriptRoot "..\..\automation\tests\BlueMsxControl.ps1")

if (-not ("MixerDevicesProbe" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class MixerDevicesProbe {
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    public static string ClassOf(IntPtr h) { var s = new StringBuilder(64); GetClassName(h, s, 64); return s.ToString(); }
    public static string TextOf(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString(); }
    public static IntPtr TopWindow(uint pid, string cls) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p);
            if (p == pid && ClassOf(h) == cls) { found = h; return false; } return true; }, IntPtr.Zero);
        return found;
    }
    public static IntPtr[] Children(IntPtr parent) {
        var list = new List<IntPtr>();
        EnumChildWindows(parent, (h, l) => { list.Add(h); return true; }, IntPtr.Zero);
        return list.ToArray();
    }
}
'@
}

$WM_COMMAND = 0x0111; $WM_HSCROLL = 0x0114; $WM_CLOSE = 0x0010
$TBM_GETPOS = 0x0400; $TBM_SETPOS = 0x0405; $BM_GETCHECK = 0x00F0
$ID_TOOLS_MIXERDEVICES = 40062

# One entry per row: its group, its label, and its controls.
function Get-DeviceRows([IntPtr]$devices) {
    $kids = [MixerDevicesProbe]::Children($devices)
    $rows = @(); $group = ""
    for ($j = 0; $j -lt $kids.Count; $j++) {
        $id = [MixerDevicesProbe]::GetDlgCtrlID($kids[$j])
        if ($id -ge 100 -and (($id - 100) % 4) -eq 0) {
            $before = $kids[$j - 2]
            if ($j -ge 2 -and ([MixerDevicesProbe]::GetDlgCtrlID($before) -band 0xffff) -eq 0xffff) {
                $group = [MixerDevicesProbe]::TextOf($before)
            }
            $rows += [pscustomobject]@{
                Group  = $group
                Label  = [MixerDevicesProbe]::TextOf($kids[$j - 1])
                On     = [int][MixerDevicesProbe]::SendMessage($kids[$j], $BM_GETCHECK, [IntPtr]::Zero, [IntPtr]::Zero)
                Volume = [int][MixerDevicesProbe]::SendMessage($kids[$j + 1], $TBM_GETPOS, [IntPtr]::Zero, [IntPtr]::Zero)
                Pan    = [int][MixerDevicesProbe]::SendMessage($kids[$j + 2], $TBM_GETPOS, [IntPtr]::Zero, [IntPtr]::Zero)
                VolumeBar = $kids[$j + 1]
            }
        }
    }
    return $rows
}

$Exe = (Resolve-Path $Exe).Path
if (-not $Keep -and (Test-Path $Root)) { Remove-Item -Recurse -Force $Root }
New-Item -ItemType Directory -Force $Root | Out-Null
$ini = Join-Path $Root "bluemsx.ini"

$args2 = @('/machine', "`"$Machine`"", '/rootdir', "`"$Root`"")
if ($Special1) { $args2 += @('/special1', "`"$Special1`"") }
if ($Special2) { $args2 += @('/special2', "`"$Special2`"") }
$args2 += $ExtraArgs
$p = Start-Process -FilePath $Exe -ArgumentList $args2 -WorkingDirectory (Split-Path $Exe) -PassThru
$null = $p.Handle

$h = Find-BlueMsxWindow $p
if ($h -eq [IntPtr]::Zero) { "no window; exit code $($p.ExitCode)"; exit 1 }
Start-Sleep -Milliseconds $BootMs

[void][MixerDevicesProbe]::PostMessage($h, $WM_COMMAND, [IntPtr]$ID_TOOLS_MIXERDEVICES, [IntPtr]::Zero)
$devices = [IntPtr]::Zero
$t0 = Get-Date
while ($devices -eq [IntPtr]::Zero -and ((Get-Date) - $t0).TotalSeconds -lt 10) {
    Start-Sleep -Milliseconds 200
    $devices = [MixerDevicesProbe]::TopWindow([uint32]$p.Id, "blueMSXMixerDevices")
}
if ($devices -eq [IntPtr]::Zero) {
    "the devices window did not open"
    [void][BlueMsxControl]::SendMsg($h, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
    exit 1
}
Start-Sleep -Milliseconds 500

"--- title: " + [MixerDevicesProbe]::TextOf($devices)
if ($Screenshot) {
    Add-Type -AssemblyName System.Drawing
    # Without this the window size is read scaled down on a high-dpi screen
    # while PrintWindow draws at full size, and the picture is cut off.
    [void][MixerDevicesProbe]::SetProcessDPIAware()
    $rect = New-Object MixerDevicesProbe+RECT
    [void][MixerDevicesProbe]::GetWindowRect($devices, [ref]$rect)
    $bmp = New-Object System.Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][MixerDevicesProbe]::PrintWindow($devices, $hdc, 2)   # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save($Screenshot, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    "--- screenshot: $Screenshot"
}
$rows = Get-DeviceRows $devices
"--- rows"
$rows | ForEach-Object { "{0,-10} {1,-24} on={2} volume={3} pan={4}" -f $_.Group, $_.Label, $_.On, $_.Volume, $_.Pan }

$target = $rows | Where-Object { $_.Group -eq $Group } | Select-Object -First 1
if ($target -and -not $Keep) {
    [void][MixerDevicesProbe]::SendMessage($target.VolumeBar, $TBM_SETPOS, [IntPtr]1, [IntPtr]$SetVolume)
    [void][MixerDevicesProbe]::SendMessage($devices, $WM_HSCROLL, [IntPtr]8, $target.VolumeBar)   # TB_ENDTRACK
    "--- set the volume of $Group / $($target.Label) to $SetVolume"
}
Start-Sleep -Milliseconds 300

[void][BlueMsxControl]::SendMsg($h, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
$exited = $p.WaitForExit(15000) -and $p.ExitCode -eq 0
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
"--- exits on WM_CLOSE: $exited"
"--- sound.mix lines in bluemsx.ini"
if (Test-Path $ini) { Get-Content $ini | Where-Object { $_ -like "sound.mix.*" } }
