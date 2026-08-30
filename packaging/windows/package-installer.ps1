[CmdletBinding()]
param(
    [ValidateSet("Release", "MinSizeRel", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [string]$SourceDir = "",
    [string]$BuildDir = "",
    [string]$OutputDir = "",
    [string]$InnoCompiler = "",
    [switch]$SkipPortableBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-ChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Parent,
        [Parameter(Mandatory = $true)][string]$Child
    )
    $parentFull = Resolve-FullPath $Parent
    $childFull = Resolve-FullPath $Child
    $prefix = $parentFull.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $childFull.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the installer output directory: $childFull"
    }
}

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $PSScriptRoot "..\.."
}
$sourceRoot = (Resolve-Path -LiteralPath $SourceDir).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $sourceRoot "build"
}
$buildRoot = Resolve-FullPath $BuildDir

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $sourceRoot "dist"
}
$distRoot = Resolve-FullPath $OutputDir

$cmakeText = Get-Content -LiteralPath (Join-Path $sourceRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match(
    $cmakeText,
    'project\s*\(\s*DailyReport\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,3})',
    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
if (-not $versionMatch.Success) {
    throw "Could not read the DailyReport version from CMakeLists.txt"
}
$version = $versionMatch.Groups[1].Value
$portableRoot = Join-Path $distRoot "DailyReport-$version-windows-x64"

if (-not $SkipPortableBuild) {
    & (Join-Path $PSScriptRoot "package-portable.ps1") `
        -Configuration $Configuration `
        -SourceDir $sourceRoot `
        -BuildDir $buildRoot `
        -OutputDir $distRoot
    if ($LASTEXITCODE -ne 0) {
        throw "The portable installation source could not be prepared."
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $portableRoot "DailyReport.exe") `
                  -PathType Leaf)) {
    throw "Installer source is missing: $portableRoot"
}

if ([string]::IsNullOrWhiteSpace($InnoCompiler)) {
    $compilerCandidates = @(
        (Join-Path $buildRoot "tools\Inno Setup 7\ISCC.exe"),
        (Join-Path $buildRoot "tools\Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 7\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 7\ISCC.exe"),
        (Join-Path ${env:LOCALAPPDATA} "Programs\Inno Setup 7\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:LOCALAPPDATA} "Programs\Inno Setup 6\ISCC.exe")
    )
    $InnoCompiler = $compilerCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($InnoCompiler) `
    -or -not (Test-Path -LiteralPath $InnoCompiler -PathType Leaf)) {
    throw "Inno Setup Compiler (ISCC.exe) was not found. Install Inno Setup 7 from https://jrsoftware.org/isdl.php and run the package command again."
}

$installerPath = Join-Path $distRoot "DailyReport-Setup-$version.exe"
$checksumPath = Join-Path $distRoot "DailyReport-Setup-$version-SHA256.txt"
Assert-ChildPath -Parent $distRoot -Child $installerPath
Assert-ChildPath -Parent $distRoot -Child $checksumPath
if (Test-Path -LiteralPath $installerPath) {
    Remove-Item -LiteralPath $installerPath -Force
}
if (Test-Path -LiteralPath $checksumPath) {
    Remove-Item -LiteralPath $checksumPath -Force
}

$innoScript = Join-Path $PSScriptRoot "DailyReport.iss"
$setupIcon = Join-Path $PSScriptRoot "dailyreport.ico"
$compilerArguments = @(
    "/Qp",
    "/DAppVersion=$version",
    "/DSourceDir=$portableRoot",
    "/DOutputDir=$distRoot",
    "/DSetupIcon=$setupIcon",
    $innoScript
)
& $InnoCompiler $compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
    throw "Inno Setup completed without producing $installerPath"
}

$sha256 = [System.Security.Cryptography.SHA256]::Create()
$installerStream = [System.IO.File]::OpenRead($installerPath)
try {
    $hashBytes = $sha256.ComputeHash($installerStream)
    $hashText = ([System.BitConverter]::ToString($hashBytes)).Replace("-", "")
} finally {
    $installerStream.Dispose()
    $sha256.Dispose()
}
[System.IO.File]::WriteAllText(
    $checksumPath,
    "$hashText  $([System.IO.Path]::GetFileName($installerPath))`r`n",
    [System.Text.Encoding]::ASCII)

$installer = Get-Item -LiteralPath $installerPath
Write-Host ""
Write-Host "Windows installer created successfully:"
Write-Host "  Setup  : $installerPath"
Write-Host "  Hash   : $checksumPath"
Write-Host "  Size   : $([math]::Round($installer.Length / 1MB, 2)) MB"
Write-Host "  SHA256 : $hashText"

