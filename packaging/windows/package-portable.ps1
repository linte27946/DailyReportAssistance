[CmdletBinding()]
param(
    [ValidateSet("Release", "MinSizeRel", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [string]$SourceDir = "",
    [string]$BuildDir = "",
    [string]$OutputDir = "",
    [switch]$SkipBuild
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
        throw "Refusing to modify a path outside the package output directory: $childFull"
    }
}

function Copy-DirectoryIfPresent {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    if (Test-Path -LiteralPath $Source -PathType Container) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
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

if (-not $SkipBuild) {
    & cmake --build $buildRoot --config $Configuration --target deploy
    if ($LASTEXITCODE -ne 0) {
        throw "Release deployment failed. Exit DailyReport from the system tray and try again."
    }
}

$binaryDir = Join-Path $buildRoot $Configuration
$applicationExe = Join-Path $binaryDir "DailyReport.exe"
if (-not (Test-Path -LiteralPath $applicationExe -PathType Leaf)) {
    throw "DailyReport.exe was not found at $applicationExe"
}

$cmakeText = Get-Content -LiteralPath (Join-Path $sourceRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match(
    $cmakeText,
    'project\s*\(\s*DailyReport\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,3})',
    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
$version = if ($versionMatch.Success) { $versionMatch.Groups[1].Value } else { "unknown" }
$packageName = "DailyReport-$version-windows-x64"
$packageRoot = Join-Path $distRoot $packageName
$zipPath = Join-Path $distRoot "$packageName.zip"
$checksumPath = Join-Path $distRoot "$packageName-SHA256.txt"

Assert-ChildPath -Parent $distRoot -Child $packageRoot
Assert-ChildPath -Parent $distRoot -Child $zipPath
Assert-ChildPath -Parent $distRoot -Child $checksumPath
New-Item -ItemType Directory -Path $distRoot -Force | Out-Null
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
if (Test-Path -LiteralPath $checksumPath) {
    Remove-Item -LiteralPath $checksumPath -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

# vcpkg's app-local deployment and the project's deploy target place all
# runtime DLLs beside the executable. PDB and other build artifacts are
# intentionally excluded from the public package.
Copy-Item -LiteralPath $applicationExe -Destination $packageRoot -Force
Get-ChildItem -LiteralPath $binaryDir -Filter "*.dll" -File |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $packageRoot -Force }

foreach ($pluginDirectory in @(
    "platforms", "sqldrivers", "styles", "imageformats", "tls",
    "networkinformation"
)) {
    Copy-DirectoryIfPresent `
        -Source (Join-Path $binaryDir $pluginDirectory) `
        -Destination (Join-Path $packageRoot $pluginDirectory)
}

$deployedMigrations = Join-Path $binaryDir "migrations"
$sourceMigrations = Join-Path $sourceRoot "src\storage\migrations"
if (Test-Path -LiteralPath $deployedMigrations -PathType Container) {
    Copy-Item -LiteralPath $deployedMigrations `
        -Destination (Join-Path $packageRoot "migrations") -Recurse -Force
} else {
    Copy-Item -LiteralPath $sourceMigrations `
        -Destination (Join-Path $packageRoot "migrations") -Recurse -Force
}

Copy-Item -LiteralPath (Join-Path $PSScriptRoot "README-Windows.txt") `
    -Destination (Join-Path $packageRoot "README-Windows.txt") -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot "README.md") `
    -Destination (Join-Path $packageRoot "README.md") -Force

# Add the MSVC runtime app-locally so recipients do not need Visual Studio or
# a separate VC++ Redistributable installation.
$vsWhere = Join-Path ${env:ProgramFiles(x86)} `
    "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
    $vsInstall = (& $vsWhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Redist.14.Latest `
        -property installationPath | Select-Object -First 1)
    if (-not [string]::IsNullOrWhiteSpace($vsInstall)) {
        $redistRoot = Join-Path $vsInstall "VC\Redist\MSVC"
        $crtDirectory = Get-ChildItem -LiteralPath $redistRoot -Directory `
                -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d' } |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC143.CRT" } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
            Select-Object -First 1
        if ($crtDirectory) {
            Get-ChildItem -LiteralPath $crtDirectory -Filter "*.dll" -File |
                ForEach-Object {
                    Copy-Item -LiteralPath $_.FullName -Destination $packageRoot -Force
                }
        }
    }
}

# Plugin dependencies are not always part of the main executable's app-local
# dependency set (for example qsqlite -> sqlite3 and qjpeg -> jpeg62). Walk the
# deployed PE files with dumpbin and close that dependency set from vcpkg/bin.
$vcpkgBin = Join-Path $buildRoot "vcpkg_installed\x64-windows\bin"
$dumpbinPath = $null
if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
    if ([string]::IsNullOrWhiteSpace($vsInstall)) {
        $vsInstall = (& $vsWhere -latest -products * -property installationPath |
            Select-Object -First 1)
    }
    if (-not [string]::IsNullOrWhiteSpace($vsInstall)) {
        $toolsRoot = Join-Path $vsInstall "VC\Tools\MSVC"
        $dumpbinPath = Get-ChildItem -LiteralPath $toolsRoot -Directory `
                -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "bin\Hostx64\x64\dumpbin.exe" } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Select-Object -First 1
    }
}

if ($dumpbinPath) {
    $knownDlls = @{}
    Get-ChildItem -LiteralPath $packageRoot -Filter "*.dll" -File -Recurse |
        ForEach-Object { $knownDlls[$_.Name.ToLowerInvariant()] = $true }
    $dependencyQueue = New-Object System.Collections.Queue
    Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
        Where-Object { $_.Extension -in ".exe", ".dll" } |
        ForEach-Object { $dependencyQueue.Enqueue($_.FullName) }

    while ($dependencyQueue.Count -gt 0) {
        $binary = [string]$dependencyQueue.Dequeue()
        $dependencyLines = & $dumpbinPath /NOLOGO /DEPENDENTS $binary
        foreach ($line in $dependencyLines) {
            $match = [regex]::Match($line, '^\s+([^\s]+\.dll)\s*$',
                [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            if (-not $match.Success) { continue }
            $dependency = $match.Groups[1].Value
            $dependencyKey = $dependency.ToLowerInvariant()
            if ($knownDlls.ContainsKey($dependencyKey)) { continue }

            $vcpkgDependency = Join-Path $vcpkgBin $dependency
            $systemDependency = Join-Path $env:WINDIR "System32\$dependency"
            if (Test-Path -LiteralPath $vcpkgDependency -PathType Leaf) {
                $destination = Join-Path $packageRoot $dependency
                Copy-Item -LiteralPath $vcpkgDependency `
                    -Destination $destination -Force
                $knownDlls[$dependencyKey] = $true
                $dependencyQueue.Enqueue($destination)
            } elseif (Test-Path -LiteralPath $systemDependency -PathType Leaf `
                      -ErrorAction SilentlyContinue) {
                $knownDlls[$dependencyKey] = $true
            } elseif ($dependencyKey.StartsWith("api-ms-win-") `
                      -or $dependencyKey.StartsWith("ext-ms-win-")) {
                $knownDlls[$dependencyKey] = $true
            } else {
                throw "Portable package dependency is missing: $dependency (required by $binary)"
            }
        }
    }
} else {
    # Conservative fallback for environments where Build Tools are present but
    # dumpbin cannot be located.
    foreach ($dependency in @("jpeg62.dll", "sqlite3.dll", "libssl-3-x64.dll")) {
        $source = Join-Path $vcpkgBin $dependency
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Required plugin dependency is missing: $source"
        }
        Copy-Item -LiteralPath $source -Destination $packageRoot -Force
    }
}

# Collect the license/copyright files installed by vcpkg. Shipping these is
# important for Qt and the other redistributed open-source runtime libraries.
$vcpkgShare = Join-Path $buildRoot "vcpkg_installed\x64-windows\share"
$licenseRoot = Join-Path $packageRoot "licenses"
if (Test-Path -LiteralPath $vcpkgShare -PathType Container) {
    New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null
    Get-ChildItem -LiteralPath $vcpkgShare -Filter "copyright" -File -Recurse |
        ForEach-Object {
            # Windows PowerShell 5.1 runs on .NET Framework, which does not
            # provide Path.GetRelativePath(). Every item was enumerated below
            # vcpkgShare, so a validated prefix trim is sufficient here.
            $relative = $_.DirectoryName.Substring($vcpkgShare.Length)
            $relative = $relative.TrimStart('\', '/')
            $licenseName = ($relative -replace '[\\/:*?"<>|]', '_') + ".txt"
            Copy-Item -LiteralPath $_.FullName `
                -Destination (Join-Path $licenseRoot $licenseName) -Force
        }
}

$requiredFiles = @(
    "DailyReport.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "Qt6Sql.dll",
    "jpeg62.dll",
    "sqlite3.dll",
    "libssl-3-x64.dll",
    "platforms\qwindows.dll",
    "sqldrivers\qsqlite.dll",
    "migrations\001_initial_schema.sql"
)
foreach ($required in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $required) -PathType Leaf)) {
        throw "Portable package validation failed; missing $required"
    }
}

Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath `
    -CompressionLevel Optimal

$zip = Get-Item -LiteralPath $zipPath
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$zipStream = [System.IO.File]::OpenRead($zipPath)
try {
    $hashBytes = $sha256.ComputeHash($zipStream)
    $hashText = ([System.BitConverter]::ToString($hashBytes)).Replace("-", "")
} finally {
    $zipStream.Dispose()
    $sha256.Dispose()
}
[System.IO.File]::WriteAllText(
    $checksumPath,
    "$hashText  $([System.IO.Path]::GetFileName($zipPath))`r`n",
    [System.Text.Encoding]::ASCII)
Write-Host ""
Write-Host "Portable package created successfully:"
Write-Host "  Folder : $packageRoot"
Write-Host "  ZIP    : $zipPath"
Write-Host "  Hash   : $checksumPath"
Write-Host "  Size   : $([math]::Round($zip.Length / 1MB, 2)) MB"
Write-Host "  SHA256 : $hashText"
