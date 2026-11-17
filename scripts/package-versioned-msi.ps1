<#
.SYNOPSIS
Assembles a version-checked, standalone x64 MSI from an existing release build.

.DESCRIPTION
Run after qmake and jom have built Moonlight.exe and AntiHooking.dll in BuildDir.
Creates fresh, version-specific dependency and WiX output directories, then archives
the MSI and a JSON manifest beside it. The source checkout must have clean tracked
files, and existing output paths are never replaced.

.EXAMPLE
powershell -NoProfile -File scripts\package-versioned-msi.ps1 -BuildDir build\series-x64-release -Version 7.1.1.1
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDir,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string] $Version,

    [string] $DeployDir,
    [string] $OutputDirectory,
    [string] $QtRoot = 'C:\local\programs\Qt\6.11.1\msvc2022_64',
    [string] $VisualStudioRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community',
    [string] $DxCDirectory = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-FullPath {
    param([string] $Path)
    return [System.IO.Path]::GetFullPath($Path)
}

function Test-IsUnderDirectory {
    param(
        [string] $Path,
        [string] $Parent
    )

    $parentPrefix = $Parent.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $Path.StartsWith($parentPrefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-RequiredFile {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
}

function Assert-RequiredDirectory {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Required directory is missing: $Path"
    }
}

function Copy-RequiredDlls {
    param(
        [string] $SourceDirectory,
        [string] $DestinationDirectory,
        [string] $Description
    )

    Assert-RequiredDirectory $SourceDirectory
    $dlls = @(Get-ChildItem -LiteralPath $SourceDirectory -Filter '*.dll' -File)
    if ($dlls.Count -eq 0) {
        throw "No DLLs were found for $Description in $SourceDirectory"
    }
    foreach ($dll in $dlls) {
        Copy-Item -LiteralPath $dll.FullName -Destination $DestinationDirectory
    }
    return $dlls.Count
}

function Get-GitOutput {
    param([string[]] $GitArguments)

    $output = @(& git -C $script:SourceRoot @GitArguments 2>&1)
    $exitCode = $LASTEXITCODE
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = ($output -join [Environment]::NewLine).Trim()
    }
}

function Get-MsiProperty {
    param(
        [object] $Database,
        [string] $Name
    )

    $query = 'SELECT `Value` FROM `Property` WHERE `Property` = ''{0}''' -f $Name
    $view = $Database.OpenView($query)
    $record = $null
    try {
        $view.Execute()
        $record = $view.Fetch()
        if ($null -eq $record) {
            throw "MSI Property table has no '$Name' value."
        }
        return [string] $record.StringData(1)
    }
    finally {
        if ($null -ne $view) {
            [void] [Runtime.InteropServices.Marshal]::FinalReleaseComObject($view)
        }
        if ($null -ne $record) {
            [void] [Runtime.InteropServices.Marshal]::FinalReleaseComObject($record)
        }
    }
}

function Get-VerifiedSourceState {
    $revisionResult = Get-GitOutput -GitArguments @('rev-parse', 'HEAD')
    if ($revisionResult.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($revisionResult.Output)) {
        throw "Unable to read source revision with git rev-parse HEAD: $($revisionResult.Output)"
    }

    $statusResult = Get-GitOutput -GitArguments @('status', '--porcelain', '--untracked-files=no')
    if ($statusResult.ExitCode -ne 0) {
        throw "Unable to verify tracked source status before packaging: $($statusResult.Output)"
    }
    if (-not [string]::IsNullOrWhiteSpace($statusResult.Output)) {
        throw "Tracked source changes must be committed before packaging. git status reported: $($statusResult.Output)"
    }

    return [pscustomobject]@{
        Revision = $revisionResult.Output
        TrackedClean = $true
        TrackedStatus = ''
    }
}

$script:SourceRoot = Get-FullPath (Join-Path $PSScriptRoot '..')
$sourceVersionFile = Join-Path $script:SourceRoot 'app\version.txt'
$versionFromSource = (Get-Content -LiteralPath $sourceVersionFile -Raw).Trim()
if ($versionFromSource -cne $Version) {
    throw "Requested version $Version does not match app/version.txt ($versionFromSource)."
}

$buildPath = Get-FullPath $BuildDir
if (-not (Test-IsUnderDirectory -Path $buildPath -Parent $script:SourceRoot)) {
    throw "BuildDir must be inside the source checkout: $script:SourceRoot"
}
Assert-RequiredDirectory $buildPath

if ([string]::IsNullOrWhiteSpace($DeployDir)) {
    $DeployDir = Join-Path $buildPath "msi-deploy-$Version"
}
$deployPath = Get-FullPath $DeployDir
if (-not (Test-IsUnderDirectory -Path $deployPath -Parent $buildPath)) {
    throw "DeployDir must be a new child directory of BuildDir: $buildPath"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = $script:SourceRoot
}
$outputPath = Get-FullPath $OutputDirectory
if (-not (Test-IsUnderDirectory -Path $outputPath -Parent $script:SourceRoot) -and
    $outputPath -ine $script:SourceRoot) {
    throw "OutputDirectory must be inside the source checkout: $script:SourceRoot"
}
Assert-RequiredDirectory $outputPath

$packageWorkPath = Join-Path $buildPath "msi-work-$Version"
$msiName = "MoonlightSetup-x64-$Version.msi"
$archivePath = Join-Path $outputPath $msiName
$manifestPath = "$archivePath.json"

foreach ($path in @($deployPath, $packageWorkPath, $archivePath, $manifestPath)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite an existing path: $path"
    }
}

$buildExe = Join-Path $buildPath 'app\release\Moonlight.exe'
$antiHookingDll = Join-Path $buildPath 'AntiHooking\release\AntiHooking.dll'
$libraryDirectory = Join-Path $script:SourceRoot 'libs\windows\lib\x64'
$gameControllerDb = Join-Path $script:SourceRoot 'app\SDL_GameControllerDB\gamecontrollerdb.txt'
$safeStartupSource = Join-Path $script:SourceRoot 'scripts\Moonlight Safe Startup.cmd'
$windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
$msbuild = Join-Path $VisualStudioRoot 'MSBuild\Current\Bin\MSBuild.exe'
$vcRedist = Join-Path $VisualStudioRoot 'VC\Redist\MSVC\14.52.37025\x64\Microsoft.VC145.CRT'
$wixProject = Join-Path $script:SourceRoot 'wix\Moonlight\Moonlight.wixproj'

foreach ($path in @($buildExe, $antiHookingDll, $gameControllerDb, $safeStartupSource, $windeployqt, $msbuild, $wixProject)) {
    Assert-RequiredFile $path
}
Assert-RequiredDirectory $libraryDirectory
Assert-RequiredDirectory $vcRedist
Assert-RequiredDirectory $DxCDirectory
foreach ($fileName in @('dxcompiler.dll', 'dxil.dll')) {
    Assert-RequiredFile (Join-Path $DxCDirectory $fileName)
}

$fileInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($buildExe)
$fileVersion = $fileInfo.FileVersion.Trim()
$productVersion = $fileInfo.ProductVersion.Trim()
if ($fileVersion -cne $Version) {
    throw "Moonlight.exe FileVersion is '$fileVersion'; expected '$Version'."
}
if ($productVersion -cne $Version) {
    throw "Moonlight.exe ProductVersion is '$productVersion'; expected '$Version'."
}
$sourceState = Get-VerifiedSourceState

New-Item -ItemType Directory -Path $deployPath -ErrorAction Stop | Out-Null
New-Item -ItemType Directory -Path $packageWorkPath -ErrorAction Stop | Out-Null
$packageObjPath = Join-Path $packageWorkPath 'obj'
$packageOutputPath = Join-Path $packageWorkPath 'output'
New-Item -ItemType Directory -Path $packageObjPath -ErrorAction Stop | Out-Null
New-Item -ItemType Directory -Path $packageOutputPath -ErrorAction Stop | Out-Null

try {
    $dependencyDllCount = Copy-RequiredDlls -SourceDirectory $libraryDirectory -DestinationDirectory $deployPath -Description 'Windows libraries'
    Copy-Item -LiteralPath $antiHookingDll -Destination $deployPath
    Copy-Item -LiteralPath $gameControllerDb -Destination $deployPath

    $windeployArgs = @(
        '--dir', $deployPath,
        '--release',
        '--qmldir', (Join-Path $script:SourceRoot 'app\gui'),
        '--no-opengl-sw',
        '--no-compiler-runtime',
        '--no-system-d3d-compiler',
        '--no-system-dxc-compiler',
        '--skip-plugin-types', 'qmltooling,generic',
        '--no-ffmpeg',
        $buildExe
    )
    & $windeployqt @windeployArgs
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE."
    }

    $qtTlsPlugin = Join-Path $QtRoot 'plugins\tls\qopensslbackend.dll'
    if (Test-Path -LiteralPath $qtTlsPlugin -PathType Leaf) {
        $tlsDirectory = Join-Path $deployPath 'tls'
        if (-not (Test-Path -LiteralPath $tlsDirectory -PathType Container)) {
            New-Item -ItemType Directory -Path $tlsDirectory -ErrorAction Stop | Out-Null
        }
        Copy-Item -LiteralPath $qtTlsPlugin -Destination $tlsDirectory
    }

    Copy-Item -LiteralPath (Join-Path $DxCDirectory 'dxcompiler.dll') -Destination $deployPath
    Copy-Item -LiteralPath (Join-Path $DxCDirectory 'dxil.dll') -Destination $deployPath
    # Standalone MSI installs need the app-local VC runtime because there is no Burn prerequisite bundle.
    $crtDllCount = Copy-RequiredDlls -SourceDirectory $vcRedist -DestinationDirectory $deployPath -Description 'Visual C++ 14.52.37025 runtime'
    Copy-Item -LiteralPath $safeStartupSource -Destination (Join-Path $buildPath 'Moonlight Safe Startup.cmd')

    if (Test-Path -LiteralPath (Join-Path $deployPath 'Moonlight.exe')) {
        throw 'DeployDir contains Moonlight.exe; WiX harvests this folder as dependencies, so the main executable must remain a separate component.'
    }
    foreach ($portableName in @('portable.dat', 'portable.dat.inactive')) {
        if (Test-Path -LiteralPath (Join-Path $deployPath $portableName)) {
            throw "DeployDir contains a portable marker: $portableName"
        }
    }
    if ((Get-ChildItem -LiteralPath $deployPath -Force | Measure-Object).Count -eq 0) {
        throw 'Qt and application dependency deployment produced an empty DeployDir.'
    }

    $savedBuildFolder = $env:BUILD_FOLDER
    $savedDeployFolder = $env:DEPLOY_FOLDER
    $savedVersion = $env:VERSION
    try {
        $env:BUILD_FOLDER = $buildPath
        $env:DEPLOY_FOLDER = $deployPath
        Remove-Item Env:\VERSION -ErrorAction SilentlyContinue

        $msbuildArgs = @(
            '-Restore',
            $wixProject,
            '/p:Configuration=release',
            '/p:Platform=x64',
            "/p:OutputPath=$packageOutputPath/",
            "/p:IntermediateOutputPath=$packageObjPath/",
            "/p:MSBuildProjectExtensionsPath=$packageObjPath/"
        )
        & $msbuild @msbuildArgs
        if ($LASTEXITCODE -ne 0) {
            throw "MSBuild failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        if ($null -eq $savedBuildFolder) { Remove-Item Env:\BUILD_FOLDER -ErrorAction SilentlyContinue } else { $env:BUILD_FOLDER = $savedBuildFolder }
        if ($null -eq $savedDeployFolder) { Remove-Item Env:\DEPLOY_FOLDER -ErrorAction SilentlyContinue } else { $env:DEPLOY_FOLDER = $savedDeployFolder }
        if ($null -eq $savedVersion) { Remove-Item Env:\VERSION -ErrorAction SilentlyContinue } else { $env:VERSION = $savedVersion }
    }

    $msiCandidates = @(Get-ChildItem -LiteralPath $packageOutputPath -Filter '*.msi' -File)
    if ($msiCandidates.Count -ne 1) {
        throw "Expected exactly one MSI in $packageOutputPath, found $($msiCandidates.Count)."
    }
    $builtMsiPath = $msiCandidates[0].FullName

    $installer = New-Object -ComObject WindowsInstaller.Installer
    $database = $null
    try {
        $database = $installer.OpenDatabase($builtMsiPath, 0)
        try {
            $msiProperties = [ordered]@{
                ProductVersion = Get-MsiProperty -Database $database -Name 'ProductVersion'
                ProductCode = Get-MsiProperty -Database $database -Name 'ProductCode'
                UpgradeCode = Get-MsiProperty -Database $database -Name 'UpgradeCode'
            }
        }
        finally {
            if ($null -ne $database) {
                [void] [Runtime.InteropServices.Marshal]::FinalReleaseComObject($database)
            }
        }
    }
    finally {
        if ($null -ne $installer) {
            [void] [Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)
        }
    }

    $expectedMsiVersionPrefix = (($Version -split '\.')[0..2] -join '.')
    if ($msiProperties.ProductVersion -cne $Version -and $msiProperties.ProductVersion -cne $expectedMsiVersionPrefix) {
        throw "MSI ProductVersion is '$($msiProperties.ProductVersion)'; expected '$Version' or its Windows Installer three-field form '$expectedMsiVersionPrefix'."
    }

    $finalSourceState = Get-VerifiedSourceState
    if ($finalSourceState.Revision -cne $sourceState.Revision) {
        throw "Source revision changed during packaging ($($sourceState.Revision) -> $($finalSourceState.Revision))."
    }

    [System.IO.File]::Copy($builtMsiPath, $archivePath, $false)
    $msiHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $exeHash = (Get-FileHash -LiteralPath $buildExe -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifest = [ordered]@{
        msiFile = $msiName
        msiSha256 = $msiHash
        exeSha256 = $exeHash
        version = $Version
        executable = [ordered]@{
            fileVersion = $fileVersion
            productVersion = $productVersion
        }
        msiProperties = $msiProperties
        source = [ordered]@{
            revision = $sourceState.Revision
            trackedClean = $sourceState.TrackedClean
            trackedStatus = $sourceState.TrackedStatus
        }
        build = [ordered]@{
            architecture = 'x64'
            configuration = 'release'
            buildDirectory = $buildPath
            dependencyDirectory = $deployPath
            qtRoot = (Get-FullPath $QtRoot)
            windeployqt = (Get-FullPath $windeployqt)
            msbuild = (Get-FullPath $msbuild)
            vcRuntimeDirectory = (Get-FullPath $vcRedist)
            dxcDirectory = (Get-FullPath $DxCDirectory)
            libraryDllCount = $dependencyDllCount
            vcRuntimeDllCount = $crtDllCount
        }
    }
    $manifestJson = ConvertTo-Json -InputObject $manifest -Depth 8
    $manifestStream = [System.IO.FileStream]::new($manifestPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $manifestBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($manifestJson + [Environment]::NewLine)
        $manifestStream.Write($manifestBytes, 0, $manifestBytes.Length)
    }
    finally {
        $manifestStream.Dispose()
    }

    Write-Host "Created $archivePath"
    Write-Host "Created $manifestPath"
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
