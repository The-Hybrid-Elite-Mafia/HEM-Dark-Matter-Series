<#
  CI only: GitHub's Windows runners have no GPU, so the UI is rendered there with Mesa's
  software OpenGL driver (llvmpipe, MIT licence) from github.com/pal1000/mesa-dist-win.
  Recent mesa-dist-win releases (26.x) no longer include the OpenGL driver, so this walks back
  through the releases, newest first, until one has a 64-bit opengl32.dll, then copies that
  folder's DLLs next to the given executable(s) so they load instead of the system opengl32.dll.
  Never shipped in the installer.
#>
param([Parameter(Mandatory = $true)][string]$TargetDirs)   # comma-separated folders
$ErrorActionPreference = 'Stop'

$work = Join-Path ([IO.Path]::GetTempPath()) 'hc-mesa'
$tags = gh release list --repo pal1000/mesa-dist-win --limit 40 --json tagName | ConvertFrom-Json | ForEach-Object { $_.tagName }
foreach ($tag in $tags) {
    $rel = gh release view $tag --repo pal1000/mesa-dist-win --json assets | ConvertFrom-Json
    $asset = $rel.assets | Where-Object { $_.name -match '^mesa3d-.*-release-msvc\.7z$' } | Select-Object -First 1
    if (-not $asset) { continue }
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    gh release download $tag --repo pal1000/mesa-dist-win --pattern $asset.name --dir $work
    & "$env:ProgramFiles\7-Zip\7z.exe" x (Join-Path $work $asset.name) "-o$work\x" -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "7z extraction failed for $tag" }
    $gl = Get-ChildItem (Join-Path $work 'x') -Recurse -Filter 'opengl32.dll' |
          Where-Object { $_.DirectoryName -match 'x64' } | Select-Object -First 1
    if (-not $gl) { Write-Host "Mesa $tag has no OpenGL driver, trying an older release"; continue }
    Write-Host "Mesa: $tag / $($asset.name) -> $($gl.DirectoryName)"
    $dlls = Get-ChildItem $gl.DirectoryName -Filter '*.dll'
    foreach ($dir in ($TargetDirs -split ',')) {
        foreach ($d in $dlls) { Copy-Item $d.FullName -Destination $dir -Force }
        Write-Host "Copied $($dlls.Count) Mesa DLLs to $dir"
    }
    exit 0
}
throw 'No mesa-dist-win release with a 64-bit opengl32.dll found'
