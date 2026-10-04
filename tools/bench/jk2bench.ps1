<#
.SYNOPSIS
  Client benchmark for jk2mv: plays demos in timedemo mode and reports fps.

.DESCRIPTION
  Implements the client protocol of docs/ROADMAP.md section 5. For every
  preset and demo, runs one warm-up and N measured runs, each in a fresh
  process, reads the timedemo result line from qconsole.log and writes all
  runs to a CSV, then prints the median fps per (preset, demo) and the
  spread between runs. A difference only counts if it is larger than
  max(3%, 2x the spread).

  Latched cvars (r_mode, r_ext_multisample, ...) are passed with +set,
  because a +exec is applied after R_Init. The profile's jk2mvconfig.cfg and
  jk2mvglobal.cfg are deleted before each run, so every run starts from the
  defaults plus its preset. Each result is appended to the CSV as soon as it
  is measured; a run that doesn't finish within -TimeoutSec is killed.

.EXAMPLE
  .\jk2bench.ps1 -Exe C:\jk2bench\builds\master\jk2mvmp.exe -HomePath C:\jk2bench\home `
      -Demos bench_ffa,bench_duel -Runs 5 -Csv C:\jk2bench\master.csv

  Demo files go to <HomePath>\base\demos\ (bench_ffa.dm_16, ...).
  Copy assets0/1/2/5.pk3 into the base folder next to jk2mvmp.exe first.
#>
param(
    [Parameter(Mandatory = $true)] [string] $Exe,
    [Parameter(Mandatory = $true)] [string] $HomePath,
    [Parameter(Mandatory = $true)] [string[]] $Demos,
    [int] $Runs = 5,
    [string] $Csv = "jk2bench.csv",
    [string[]] $Presets = @("cpu", "classic", "gpu"),
    [int] $TimeoutSec = 600
)

$ErrorActionPreference = "Stop"
$inv = [Globalization.CultureInfo]::InvariantCulture

# absolute paths: the game runs from its own folder
$Exe = (Resolve-Path $Exe).ProviderPath
New-Item -ItemType Directory -Force $HomePath | Out-Null
$HomePath = (Resolve-Path $HomePath).ProviderPath
$Csv = [IO.Path]::GetFullPath((Join-Path (Get-Location).ProviderPath $Csv))
# powershell -File passes "-Demos a,b" as one string
$Demos = @($Demos | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
foreach ($demo in $Demos) {
    $base = Join-Path $HomePath "base\demos\$demo"
    if (-not ((Test-Path "$base.dm_15") -or (Test-Path "$base.dm_16"))) { throw "no demo $base.dm_15 or .dm_16" }
}

# +set arguments for each measurement preset
$PresetArgs = @{
    # small resolution: the CPU side dominates
    "cpu"     = "+set r_mode -1 +set r_customwidth 640 +set r_customheight 480 +set r_ext_multisample 0 +set r_ext_texture_filter_anisotropic 2 +set r_DynamicGlow 0"
    # 1080p, default look
    "classic" = "+set r_mode -1 +set r_customwidth 1920 +set r_customheight 1080 +set r_ext_multisample 0 +set r_ext_texture_filter_anisotropic 2 +set r_DynamicGlow 0"
    # 4K with the expensive options: the GPU side dominates
    "gpu"     = "+set r_mode -1 +set r_customwidth 3840 +set r_customheight 2160 +set r_ext_multisample 4 +set r_ext_texture_filter_anisotropic 16 +set r_DynamicGlow 1"
}

$log = Join-Path $HomePath "base\qconsole.log"
$configs = @("jk2mvconfig.cfg", "jk2mvglobal.cfg") | ForEach-Object { Join-Path $HomePath "base\$_" }
$common = "+set fs_homepath `"$HomePath`" +set logfile 2 +set r_fullscreen 1 +set r_swapInterval 0 +set com_maxfps 0 +set s_initsound 0"
$results = @()

function Invoke-Run([string] $preset, [string] $demo) {
    if (Test-Path $log) { Remove-Item $log }
    # settings archived by the previous run would leak into this one
    foreach ($cfg in $configs) { if (Test-Path $cfg) { Remove-Item $cfg } }
    $arguments = "$common $($PresetArgs[$preset]) +set timedemo 1 +set nextdemo quit +demo $demo"
    $p = Start-Process -FilePath $Exe -ArgumentList $arguments -WorkingDirectory (Split-Path $Exe) -PassThru
    # a demo that fails to load leaves the game in the menu
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        $p.Kill()
        throw "$demo ($preset) did not finish within $TimeoutSec s, see $log"
    }
    if (-not (Test-Path $log)) { throw "no qconsole.log after running $demo ($preset)" }
    $line = Select-String -Path $log -Pattern '(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps' | Select-Object -Last 1
    if (-not $line) { throw "no timedemo result in $log for $demo ($preset)" }
    $m = $line.Matches[0]
    return [pscustomobject]@{
        preset = $preset; demo = $demo
        frames = [int]$m.Groups[1].Value
        seconds = [double]::Parse($m.Groups[2].Value, $inv)
        fps = [double]::Parse($m.Groups[3].Value, $inv)
    }
}

# written by hand: Export-Csv formats numbers in the current culture
# ("123,4" on a French system, which other tools misread)
Set-Content -Path $Csv -Value "preset,demo,run,frames,seconds,fps" -Encoding ascii
function Add-CsvRow($r) {
    $row = "{0},{1},{2},{3},{4},{5}" -f $r.preset, $r.demo, $r.run, $r.frames,
        $r.seconds.ToString("0.000", $inv), $r.fps.ToString("0.0", $inv)
    Add-Content -Path $Csv -Value $row -Encoding ascii
}

foreach ($preset in $Presets) {
    if (-not $PresetArgs.ContainsKey($preset)) { throw "unknown preset $preset" }
    foreach ($demo in $Demos) {
        Write-Host "== $preset / $demo : warm-up"
        Invoke-Run $preset $demo | Out-Null
        for ($i = 1; $i -le $Runs; $i++) {
            $r = Invoke-Run $preset $demo
            $r | Add-Member run $i
            Write-Host ("   run {0}: {1:N1} fps" -f $i, $r.fps)
            Add-CsvRow $r
            $results += $r
        }
    }
}

Write-Host "`nAll runs written to $Csv`n"

$results | Group-Object preset, demo | ForEach-Object {
    $fps = $_.Group.fps | Sort-Object
    $median = if ($fps.Count % 2) { $fps[[int][math]::Floor($fps.Count / 2)] } else { ($fps[$fps.Count / 2 - 1] + $fps[$fps.Count / 2]) / 2 }
    $spread = if ($median -gt 0) { 100 * (($fps | Measure-Object -Maximum).Maximum - ($fps | Measure-Object -Minimum).Minimum) / $median } else { 0 }
    [pscustomobject]@{ "preset / demo" = $_.Name; "median fps" = [math]::Round($median, 1); "spread %" = [math]::Round($spread, 1) }
} | Format-Table -AutoSize

Write-Host "timedemo caps at 1000 fps and counts whole frames; the in-game 'benchmark' command gives frame-time percentiles."
