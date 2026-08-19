# Builds and runs FidelityReference, then copies its exported PNGs into
# windows_fidelity_reference/output/ — mirrors ios_fidelity_reference's
# export_references.sh: batch-export in one process run, then exit(0)
# (see MainWindow.xaml.cs's OnActivatedOnce), which this script waits on
# via the plain synchronous process launch below.
#
# Must build with Visual Studio's own MSBuild.exe (VS Community's "Windows
# application development"/UWP workload), not `dotnet build`/`dotnet
# publish` — several WindowsAppSDK build targets (PRI/resource-index
# generation among them) hard-require Visual Studio's own AppxPackage
# MSBuild tooling with no clean opt-out. Confirmed: `dotnet` CLI builds look
# for that tooling under the dotnet SDK's own install tree, where it never
# exists; VS's MSBuild.exe looks under VS's own install tree, where it does
# (once the Universal/UWP workload is installed).

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$OutputDir = Join-Path $ScriptDir "output"

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallPath = & $vswhere -latest -requires Microsoft.VisualStudio.Workload.Universal -property installationPath
if (-not $vsInstallPath) {
    Write-Error "No Visual Studio installation found with the Universal (UWP) workload - required for WindowsAppSDK's AppxPackage MSBuild tooling."
    exit 1
}
$msbuild = Join-Path $vsInstallPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Error "Could not find MSBuild.exe at $msbuild"
    exit 1
}

Write-Output "Building FidelityReference (via $msbuild)..."
# Plain Build, not Publish — Publish's self-contained deployment path
# crashes on startup (STATUS_STOWED_EXCEPTION in Microsoft.UI.Xaml.dll,
# confirmed via Windows Event Log). A plain Build with
# WindowsAppSDKSelfContained=true (set in the .csproj) already copies every
# runtime DLL this app needs alongside the exe and runs correctly — this
# app is dev-only, always run directly out of the build tree, never
# redistributed, so Publish's extra packaging isn't needed anyway.
& $msbuild FidelityReference.csproj -restore -p:Configuration=Release -t:Build -v:minimal
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

$Exe = Join-Path $ScriptDir "bin\Release\net8.0-windows10.0.19041.0\win-x64\FidelityReference.exe"
if (-not (Test-Path $Exe)) {
    Write-Error "Could not find built FidelityReference.exe at $Exe"
    exit 1
}

# One process run per theme, not a single "fluent_light,fluent_dark" run —
# Application.Current.RequestedTheme (which ComponentCatalog.cs's direct
# Application.Current.Resources[key] lookups depend on for ~23 builders) can
# only be set once, before any window exists (see App.xaml.cs's
# constructor); it can't be switched mid-process. Each run's single theme
# name doubles as both the app-level theme selector (App.xaml.cs) and the
# ElementTheme/subfolder selector (MainWindow.xaml.cs) — same convention as
# before, just one at a time now.
$SrcDir = Join-Path (Split-Path $Exe) "fidelity_output"
if (Test-Path $OutputDir) { Remove-Item -Recurse -Force $OutputDir }

foreach ($theme in @("fluent_light", "fluent_dark")) {
    Write-Output "Running export ($theme)..."
    if (Test-Path $SrcDir) { Remove-Item -Recurse -Force $SrcDir }

    # Start-Process -Wait, not `&` — $LASTEXITCODE after `&` is unreliable for
    # a WinExe-subsystem (GUI) process launched this way (confirmed: the run
    # completes and produces every fidelity_output PNG correctly, but `&`'s
    # $LASTEXITCODE read back nonzero anyway). -Wait blocks until the process
    # has actually exited, which OnActivatedOnce's Environment.Exit(0) call
    # guarantees happens once the export loop finishes.
    $proc = Start-Process -FilePath $Exe -ArgumentList "--themes=$theme" -Wait -PassThru
    if ($proc.ExitCode -ne 0) { Write-Error "Export run failed for ${theme} (exit $($proc.ExitCode))"; exit 1 }

    if (-not (Test-Path $SrcDir)) {
        Write-Error "No fidelity_output found at $SrcDir"
        exit 1
    }

    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    Copy-Item -Recurse -Force (Join-Path $SrcDir $theme) (Join-Path $OutputDir $theme)
}

$Count = (Get-ChildItem -Recurse -Filter *.png $OutputDir).Count
Write-Output "Exported $Count reference screenshots to $OutputDir"
