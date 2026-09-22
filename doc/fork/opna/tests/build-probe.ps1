param(
    [Parameter(Mandatory = $true)][string]$Src,
    [string]$Glue = "",
    [string]$Out = "opna-probe.exe"
)
# Builds opna-probe.exe against blueMSX/Src. -Glue replaces YM2608.cpp with
# another copy (a deliberately broken one), which is how the probe is shown
# to catch what it claims to check.
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($Glue -eq "") { $Glue = Join-Path $Src "SoundChips\YM2608.cpp" }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -products * -version "[17.0,18.0)" -property installationPath | Select-Object -First 1
$inc = "Common SoundChips Board Debugger Utils Media Memory Emulator Arch IoDevice VideoChips Input Z80 Language".Split(" ") |
    ForEach-Object { "/I`"" + (Join-Path $Src $_) + "`"" }
$ymfm = "ymfm_opn.cpp ymfm_adpcm.cpp ymfm_ssg.cpp".Split(" ") |
    ForEach-Object { "`"" + (Join-Path $Src "SoundChips\ymfm\$_") + "`"" }
$objDir = Join-Path $here ("obj-" + [IO.Path]::GetFileNameWithoutExtension($Out))
New-Item -ItemType Directory -Force $objDir | Out-Null
$cl = "cl /nologo /W3 /O2 /EHsc /D_CRT_SECURE_NO_WARNINGS $($inc -join ' ') " +
      "opna-probe.cpp opna-host-stub.cpp `"$Glue`" $($ymfm -join ' ') /Fo`"$objDir\\`" /Fe:`"$Out`""
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul 2>&1 && cd /d `"$here`" && $cl" | Select-String -Pattern " error | warning " | ForEach-Object { $_.Line }
exit $LASTEXITCODE
