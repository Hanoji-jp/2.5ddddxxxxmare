# ============================================================
# shrink_audio.ps1
#   Asset/Sound 以下の mp3 を ffmpeg で低ビットレートへ再エンコードして
#   容量を削減する。元音源は _audio_original/ に保管し、常にそこから
#   エンコードするので、何度実行しても劣化が累積しない（復元も可能）。
#
#   BGM  : 128 kbps（音楽）
#   SE   :  96 kbps（効果音）
#
#   要 ffmpeg（PATH 上にあること）。
#   復元: _audio_original/ の中身を Asset/Sound/ へ戻す。
# ============================================================
$ErrorActionPreference = "Stop"
$root     = Split-Path -Parent $PSScriptRoot
$soundDir = Join-Path $root "Asset\Sound"
$backup   = Join-Path $root "_audio_original"   # Asset の外（pakに入らない）

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    Write-Error "ffmpeg が見つかりません。PATH に通してください。"
    exit 1
}
if (-not (Test-Path $soundDir)) { Write-Error "Asset\Sound がありません"; exit 1 }

$files = Get-ChildItem -Path $soundDir -Recurse -File -Filter *.mp3

$before = 0; $after = 0
foreach ($f in $files) {
    $rel = $f.FullName.Substring($soundDir.Length).TrimStart('\','/')
    $bak = Join-Path $backup $rel

    # 初回のみ元をバックアップ。以降は常にバックアップ(pristine)からエンコード。
    if (-not (Test-Path $bak)) {
        New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
        Copy-Item $f.FullName $bak -Force
    }

    $before += (Get-Item $bak).Length

    # BGM/SE でビットレートを変える
    $rate = if ($rel -match '(?i)\\?SE\\') { "96k" } else { "128k" }

    $tmp = "$($f.FullName).tmp.mp3"
    & ffmpeg -y -loglevel error -i $bak -map_metadata -1 -c:a libmp3lame -b:a $rate $tmp
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $tmp)) {
        Write-Warning "skip (encode failed): $rel"
        if (Test-Path $tmp) { Remove-Item $tmp -Force }
        continue
    }
    Move-Item $tmp $f.FullName -Force
    $after += (Get-Item $f.FullName).Length
}

$bMB = [math]::Round($before/1MB,1)
$aMB = [math]::Round($after/1MB,1)
Write-Host "[shrink_audio] $($files.Count) files : $bMB MB -> $aMB MB  (backup: _audio_original)"
