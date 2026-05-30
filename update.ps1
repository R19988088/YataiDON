# YataiDON Windows Updater
#
# Expected GitHub release assets:
#   checksums-windows.sha256    sha256sum-format, relative paths from install dir
#                               (excludes git-managed skins)
#   skin-manifest.tsv           path<TAB>repo_url<TAB>commit<TAB>checksums_url
#   update-windows.tar.gz       binary + dlls + shader + bundled skins
#
# Skin repos must contain a checksums.sha256 at their root.
#
# Usage (standalone):   powershell -ExecutionPolicy Bypass -File update.ps1
# Usage (from game):    update.bat --wait-pid <PID>

param(
    [string]$WaitPid = ""
)

$ErrorActionPreference = "Stop"

$Repo        = "yonokid/YataiDON"
$ApiUrl      = "https://api.github.com/repos/$Repo/releases/latest"
$InstallDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$VersionFile = Join-Path $InstallDir ".version"
$TmpDir      = Join-Path $env:TEMP "YataiDON-update-$([System.IO.Path]::GetRandomFileName())"
New-Item -ItemType Directory -Path $TmpDir | Out-Null

function Log { param($msg) Write-Host "[update] $msg" }
function Die { param($msg) Write-Host "[update] Error: $msg" -ForegroundColor Red; exit 1 }

function Get-SkinRawUrl {
    param([string]$RepoUrl, [string]$Commit, [string]$FilePath)
    $base = $RepoUrl -replace '\.git$', ''
    if ($base -match 'github\.com') {
        $base = $base -replace 'github\.com', 'raw.githubusercontent.com'
        return "$base/$Commit/$FilePath"
    } else {
        return "$base/raw/commit/$Commit/$FilePath"
    }
}

try {
    # --- Fetch release metadata ---
    Log "Checking for updates..."
    $Release   = Invoke-RestMethod -Uri $ApiUrl -TimeoutSec 10
    $LatestTag = $Release.tag_name
    $LocalTag  = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { "none" }
    Log "Local: $LocalTag | Latest: $LatestTag"

    $AssetMap = @{}
    foreach ($asset in $Release.assets) { $AssetMap[$asset.name] = $asset.browser_download_url }

    # --- Download checksums ---
    if (-not $AssetMap.ContainsKey("checksums-windows.sha256")) {
        Die "No checksums-windows.sha256 in release $LatestTag"
    }
    $ChecksumsPath = Join-Path $TmpDir "checksums.sha256"
    Invoke-WebRequest -Uri $AssetMap["checksums-windows.sha256"] -OutFile $ChecksumsPath -TimeoutSec 30

    # --- Download skin manifest ---
    $SkinManifestPath = Join-Path $TmpDir "skin-manifest.tsv"
    if ($AssetMap.ContainsKey("skin-manifest.tsv")) {
        Invoke-WebRequest -Uri $AssetMap["skin-manifest.tsv"] -OutFile $SkinManifestPath -TimeoutSec 10
    } else {
        New-Item -ItemType File -Path $SkinManifestPath | Out-Null
    }

    # --- Check main package ---
    $NeedPackage = $false
    foreach ($line in Get-Content $ChecksumsPath) {
        $parts = $line -split '\s+', 2
        if ($parts.Count -lt 2) { continue }
        $expectedHash = $parts[0].ToUpper()
        $relPath      = $parts[1].Replace('/', '\')
        $localFile    = Join-Path $InstallDir $relPath

        if (Test-Path $localFile) {
            $actualHash = (Get-FileHash $localFile -Algorithm SHA256).Hash
            if ($actualHash -ne $expectedHash) { $NeedPackage = $true; break }
        } else {
            $NeedPackage = $true; break
        }
    }

    # --- Check installed skins against manifest ---
    $NeedSkinCount = 0
    foreach ($line in Get-Content $SkinManifestPath) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line -split "`t"
        if ($parts.Count -lt 4) { continue }
        $skinPath, $repoUrl, $expectedCommit, $checksumsUrl = $parts

        $localVersionFile = Join-Path $InstallDir $skinPath ".skin-version"
        $localCommit = if (Test-Path $localVersionFile) { (Get-Content $localVersionFile -Raw).Trim() } else { "none" }
        if ($localCommit -ne $expectedCommit) { $NeedSkinCount++ }
    }

    if (-not $NeedPackage -and $NeedSkinCount -eq 0) {
        Log "Already up to date."
        Set-Content $VersionFile $LatestTag
        exit 0
    }

    Log "Updates needed — package: $([int]$NeedPackage) | skins: $NeedSkinCount"

    # --- Wait for game process if requested ---
    if ($WaitPid -ne "") {
        Log "Waiting for game (PID $WaitPid) to exit..."
        $proc = Get-Process -Id ([int]$WaitPid) -ErrorAction SilentlyContinue
        if ($proc) { $proc.WaitForExit() }
    }

    # --- Download and extract main package ---
    if ($NeedPackage) {
        if (-not $AssetMap.ContainsKey("update-windows.tar.gz")) {
            Die "No update-windows.tar.gz in release $LatestTag"
        }
        $TarPath = Join-Path $TmpDir "update-windows.tar.gz"
        Log "Downloading update-windows.tar.gz..."
        Invoke-WebRequest -Uri $AssetMap["update-windows.tar.gz"] -OutFile $TarPath
        Log "Extracting..."
        & tar -xzf $TarPath -C $InstallDir
        if ($LASTEXITCODE -ne 0) { Die "tar extraction failed" }
        Log "Package applied."
    }

    # --- Update skins ---
    if ($NeedSkinCount -gt 0) {
        foreach ($line in Get-Content $SkinManifestPath) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $parts = $line -split "`t"
            if ($parts.Count -lt 4) { continue }
            $skinPath, $repoUrl, $expectedCommit, $checksumsUrl = $parts

            $localDir         = Join-Path $InstallDir $skinPath
            $localVersionFile = Join-Path $localDir ".skin-version"
            $localCommit = if (Test-Path $localVersionFile) { (Get-Content $localVersionFile -Raw).Trim() } else { "none" }
            if ($localCommit -eq $expectedCommit) { continue }

            Log "Checking $skinPath..."
            $skinChecksumsPath = Join-Path $TmpDir "skin-checksums.sha256"
            try {
                Invoke-WebRequest -Uri $checksumsUrl -OutFile $skinChecksumsPath -TimeoutSec 30
            } catch {
                Log "Warning: could not fetch checksums for $skinPath — skipping"
                continue
            }

            $changed = 0
            foreach ($cline in Get-Content $skinChecksumsPath) {
                $cparts = $cline -split '\s+', 2
                if ($cparts.Count -lt 2) { continue }
                $expectedHash = $cparts[0].ToUpper()
                $relPath      = $cparts[1]
                $localFile    = Join-Path $localDir ($relPath.Replace('/', '\'))

                if (Test-Path $localFile) {
                    $actualHash = (Get-FileHash $localFile -Algorithm SHA256).Hash
                    if ($actualHash -eq $expectedHash) { continue }
                }

                $rawUrl = Get-SkinRawUrl -RepoUrl $repoUrl -Commit $expectedCommit -FilePath $relPath
                $parentDir = Split-Path -Parent $localFile
                if (-not (Test-Path $parentDir)) { New-Item -ItemType Directory -Path $parentDir | Out-Null }
                try {
                    Invoke-WebRequest -Uri $rawUrl -OutFile "$localFile.tmp"
                    Move-Item -Force "$localFile.tmp" $localFile
                    $changed++
                } catch {
                    Log "Warning: failed to download $relPath"
                    Remove-Item -Force "$localFile.tmp" -ErrorAction SilentlyContinue
                }
            }

            Set-Content $localVersionFile $expectedCommit
            Log "$skinPath`: $changed file(s) updated."
        }
    }

    Set-Content $VersionFile $LatestTag
    Log "Update complete ($LatestTag). Restart YataiDON to apply."

} finally {
    Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue
}
