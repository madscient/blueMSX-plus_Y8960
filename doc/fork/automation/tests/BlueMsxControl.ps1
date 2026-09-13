# Helpers for driving blueMSX+ from PowerShell: find its window, press keys in
# the MSX key matrix, and send menu commands. Dot-source this file.
#
# The window of a /hidden instance has no MainWindowHandle, so it is looked up
# by class and process.

$env:LIB = ""       # Add-Type stops when LIB names a missing directory
$env:INCLUDE = ""

if (-not ("BlueMsxControl" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class BlueMsxControl {
    [DllImport("user32.dll", CharSet = CharSet.Ansi)] static extern IntPtr FindWindowEx(IntPtr p, IntPtr a, string cls, string win);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
    [StructLayout(LayoutKind.Sequential)] struct COPYDATASTRUCT { public IntPtr dwData; public int cbData; public IntPtr lpData; }
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, ref COPYDATASTRUCT c);
    [DllImport("user32.dll", EntryPoint = "SendMessage")] public static extern IntPtr SendMsg(IntPtr h, uint msg, IntPtr w, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr l);

    public static IntPtr Find(uint pid) {
        IntPtr h = IntPtr.Zero;
        while ((h = FindWindowEx(IntPtr.Zero, h, "blueMSX", null)) != IntPtr.Zero) {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p == pid) return h;
        }
        return IntPtr.Zero;
    }

    // -1 when the window is gone: a refused request also answers 0, and a
    // script must not read an emulator that exited as one that said no.
    public static long CopyData(IntPtr h, long id, string text) {
        if (!IsWindow(h)) return -1;
        COPYDATASTRUCT c = new COPYDATASTRUCT();
        c.dwData = new IntPtr(id);
        byte[] b = Encoding.ASCII.GetBytes(text + "\0");
        IntPtr mem = Marshal.AllocHGlobal(b.Length);
        try {
            Marshal.Copy(b, 0, mem, b.Length);
            c.cbData = b.Length; c.lpData = mem;
            return SendMessage(h, 0x004A, IntPtr.Zero, ref c).ToInt64();
        } finally { Marshal.FreeHGlobal(mem); }
    }

    public static int VisibleWindows(uint pid) {
        int n = 0;
        EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) n++; return true; }, IntPtr.Zero);
        return n;
    }
}
'@
}

$BlueMsxKeyMatrixId = 0x424D5854
$BlueMsxLaunchId    = 0x424D5846
$BlueMsxMenu = @{ LogWav = 40016; Screenshot = 40017 }

function Find-BlueMsxWindow([System.Diagnostics.Process]$Process, [int]$TimeoutMs = 10000) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalMilliseconds -lt $TimeoutMs) {
        if ($Process.HasExited) { return [IntPtr]::Zero }
        $h = [BlueMsxControl]::Find([uint32]$Process.Id)
        if ($h -ne [IntPtr]::Zero) { return $h }
        Start-Sleep -Milliseconds 100
    }
    return [IntPtr]::Zero
}

# Returns 1 when the whole request was queued, 0 when it was refused, and -1
# when the emulator's window no longer exists.
function Send-KeyMatrix([IntPtr]$Window, [string]$Commands) {
    return [BlueMsxControl]::CopyData($Window, $BlueMsxKeyMatrixId, $Commands)
}

function Get-KeyMatrixRemaining([IntPtr]$Window) {
    return [BlueMsxControl]::CopyData($Window, $BlueMsxKeyMatrixId, "")
}

# True when the queue has drained; false on timeout or when the emulator is gone.
function Wait-KeyMatrix([IntPtr]$Window, [int]$TimeoutSeconds = 60) {
    $t0 = Get-Date
    while ($true) {
        $n = Get-KeyMatrixRemaining $Window
        if ($n -eq 0) { return $true }
        if ($n -lt 0 -or ((Get-Date) - $t0).TotalSeconds -gt $TimeoutSeconds) { return $false }
        Start-Sleep -Milliseconds 100
    }
}

function Invoke-BlueMsxMenu([IntPtr]$Window, [int]$CommandId) {
    [void][BlueMsxControl]::SendMsg($Window, 0x0111, [IntPtr]$CommandId, [IntPtr]::Zero)
}

# Character -> row, mask, shift for the international layout (C-BIOS US).
# Letters, digits, space, RETURN and the symbols of rows 0-2.
function Get-KeyInternational([char]$c) {
    $letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    $s = [string]$c
    $u = $s.ToUpper()
    $i = $letters.IndexOf($u)
    if ($i -ge 0) {
        # A and B end row 2; C onwards fill rows 3-5 from bit 0.
        if ($i -lt 2) { $row = 2; $bit = 6 + $i } else { $row = 3 + [math]::Floor(($i - 2) / 8); $bit = ($i - 2) % 8 }
        return @($row, (1 -shl $bit), [int]($s -cmatch '[A-Z]'))
    }
    if ($s -match '^[0-7]$') { return @(0, (1 -shl [int]$s), 0) }
    if ($s -match '^[89]$')  { return @(1, (1 -shl ([int]$s - 8)), 0) }
    # Rows 0-2 hold the symbols; each entry is row, bit, shift.
    $symbols = @{
        ')' = @(0,0,1); '!' = @(0,1,1); '@' = @(0,2,1); '#' = @(0,3,1); '$' = @(0,4,1)
        '%' = @(0,5,1); '^' = @(0,6,1); '&' = @(0,7,1); '*' = @(1,0,1); '(' = @(1,1,1)
        '-' = @(1,2,0); '_' = @(1,2,1); '=' = @(1,3,0); '+' = @(1,3,1); ';' = @(1,7,0)
        ':' = @(1,7,1); "'" = @(2,0,0); '"' = @(2,0,1); ',' = @(2,2,0); '<' = @(2,2,1)
        '.' = @(2,3,0); '>' = @(2,3,1); '/' = @(2,4,0); '?' = @(2,4,1)
    }
    if ($symbols.ContainsKey($s)) { $e = $symbols[$s]; return @($e[0], (1 -shl $e[1]), $e[2]) }
    switch ($s) {
        ' '  { return @(8, 0x01, 0) }
        "`n" { return @(7, 0x80, 0) }
    }
    throw "no key for '$s'"
}

# The MSX BIOS scans the keyboard only every few interrupts: a 40 ms press lost
# characters in a test, 80 ms did not.
function ConvertTo-KeyMatrix([string]$Text, [int]$HoldMs = 80) {
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($c in $Text.ToCharArray()) {
        $k = Get-KeyInternational $c
        if ($k[2]) { $out.Add("d 6 0x01") }
        $out.Add(("d {0} 0x{1:x2} w {2}" -f $k[0], $k[1], $HoldMs))
        $out.Add(("u {0} 0x{1:x2}" -f $k[0], $k[1]))
        if ($k[2]) { $out.Add("u 6 0x01") }
        $out.Add("w $HoldMs")
    }
    return ($out -join " ")
}
