<#
  CI: builds the Inno Setup installer from the Release build (VST3 bundle + self-tests).
  Version: the VERSION file at the repo root. Builds of main use it as-is; any other branch gets
  "-dev.<run number>".
#>
param([string]$BuildDir = 'build')
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$bin  = Join-Path $root $BuildDir

$base = (Get-Content (Join-Path $root 'VERSION') -Raw).Trim()
if ($base -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION must be x.y.z, got '$base'" }
if ($env:GITHUB_REF -eq 'refs/heads/main') { $ver = $base } else { $ver = "$base-dev.$($env:GITHUB_RUN_NUMBER)" }

$iscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
if (-not (Test-Path $iscc)) {
    Write-Host 'Inno Setup not found on the runner; installing it with Chocolatey.'
    choco install innosetup -y --no-progress | Out-Host
}
if (-not (Test-Path $iscc)) { throw "ISCC.exe not found at $iscc" }

$bundle = Get-ChildItem $bin -Recurse -Directory -Filter 'HEM Dark Matter.vst3' | Where-Object { $_.FullName -match '\\Release\\' } | Select-Object -First 1
if (-not $bundle) { throw 'HEM Dark Matter.vst3 not found in the Release build' }
$vstLic = Get-ChildItem (Join-Path $bin '_deps') -Recurse -Filter 'LICENSE.txt' | Where-Object { $_.Directory.Name -eq 'vst3sdk-src' } | Select-Object -First 1
if (-not $vstLic) { throw 'VST3 SDK LICENSE.txt not found' }
Write-Host "Bundle: $($bundle.FullName)"

& $iscc "/DAppVersion=$ver" "/DBundle=$($bundle.FullName)" "/DBinDir=$bin" "/DVst3License=$($vstLic.FullName)" (Join-Path $root 'installer\HEMDarkMatter.iss') | Out-Host
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

$exe = Get-ChildItem (Join-Path $root 'installer\output') -Filter 'HEM-Dark-Matter-Setup-*.exe' | Select-Object -First 1
Write-Host ("Installer: {0}  ({1:N1} MB)" -f $exe.Name, ($exe.Length / 1MB))
$sha = (Get-FileHash $exe.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "SHA-256: $sha"
"DM_INSTALLER=$($exe.FullName)" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
"DM_VERSION=$ver"               | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
"DM_SHA256=$sha"                | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
