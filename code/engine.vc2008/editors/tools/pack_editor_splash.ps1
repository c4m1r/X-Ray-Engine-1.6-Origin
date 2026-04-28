# Packs JPGs from editors/images/Splash into a ZIP container named SplashImages.db_e next to the linker output.
param(
    [Parameter(Mandatory = $false)]
    [string] $OutputDir
)

function Write-SplashWarning {
    param([string] $Message)
    [Console]::Error.WriteLine("pack_editor_splash.ps1: $Message")
}

$OutputDir = if ($null -eq $OutputDir) { "" } else { $OutputDir.Trim() }
$OutputDir = $OutputDir.TrimEnd('\', '/')
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    exit 0
}

try {
    $OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
}
catch {
    Write-SplashWarning $_.Exception.Message
    exit 0
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    exit 0
}

if (-not (Test-Path -LiteralPath $OutputDir)) {
    try {
        New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    }
    catch {
        Write-SplashWarning "cannot create output folder: $($_.Exception.Message)"
        exit 0
    }
}

$editorsRoot = Split-Path -Parent $PSScriptRoot
$src         = Join-Path $editorsRoot "images\Splash"
if (-not (Test-Path -LiteralPath $src)) {
    exit 0
}

$jpg = @(Get-ChildItem -LiteralPath $src -Filter *.jpg -File -ErrorAction SilentlyContinue)
if ($jpg.Count -eq 0) {
    exit 0
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$dst = Join-Path $OutputDir "SplashImages.db_e"
try {
    if (Test-Path -LiteralPath $dst) {
        Remove-Item -LiteralPath $dst -Force -ErrorAction Stop
    }

    $archive = [System.IO.Compression.ZipFile]::Open(
        $dst,
        [System.IO.Compression.ZipArchiveMode]::Create
    )

    try {
        foreach ($f in $jpg) {
            $entryName = [System.IO.Path]::GetFileName($f.FullName)
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive,
                $f.FullName,
                $entryName,
                [System.IO.Compression.CompressionLevel]::Optimal
            )
        }
    }
    finally {
        $archive.Dispose()
    }
}
catch {
    Write-SplashWarning $_.Exception.Message
    exit 0
}

exit 0
