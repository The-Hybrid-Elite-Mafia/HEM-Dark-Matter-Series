<#
  CI only: GitHub's Windows runners have no GPU, so the UI is rendered there with Mesa's
  software OpenGL driver (llvmpipe, MIT licence) from github.com/pal1000/mesa-dist-win.
  Copies its x64 DLLs next to the given executable(s) so they load instead of the
  system opengl32.dll. Never shipped in the installer.
#>
param([Parameter(Mandatory = $true)][string]$TargetDirs)   # comma-separated folders
$ErrorActionPreference = 'Stop'

$work = Join-Path ([IO.Path]::GetTempPath()) 'hc-mesa'
Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $work | Out-Null

$assets = gh release view --repo pal1000/mesa-dist-win --json tagName,assets | ConvertFrom-Json
$asset  = $assets.assets | Where-Object { $_.name -match '^mesa3d-.*-release-msvc\.7z$' } | Select-Object -First 1
if (-not $asset) { throw "No release-msvc 7z asset found in pal1000/mesa-dist-win $($assets.tagName)" }
Write-Host "Mesa: $($assets.tagName) / $($asset.name)"

gh release download $assets.tagName --repo pal1000/mesa-dist-win --pattern $asset.name --dir $work
$archive = Join-Path $work $asset.name
& "$env:ProgramFiles\7-Zip\7z.exe" x $archive "-o$work\x" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw "7z extraction failed" }

# The archive layout moves between releases: find the 64-bit opengl32.dll wherever it sits.
$gl = Get-ChildItem (Join-Path $work 'x') -Recurse -Filter 'opengl32.dll' |
      Where-Object { $_.DirectoryName -match '(x64|amd64|64)' } | Select-Object -First 1
if (-not $gl) {
    Get-ChildItem (Join-Path $work 'x') -Recurse -Filter '*.dll' | ForEach-Object { Write-Host "  found: $($_.FullName)" }
    throw 'opengl32.dll (64-bit) not found in the Mesa archive'
}
Write-Host "Using Mesa DLLs from $($gl.DirectoryName)"
$dlls = Get-ChildItem $gl.DirectoryName -Filter '*.dll'
foreach ($dir in ($TargetDirs -split ',')) {
    foreach ($d in $dlls) { Copy-Item $d.FullName -Destination $dir -Force }
    Write-Host "Copied $($dlls.Count) Mesa DLLs to $dir"
}
