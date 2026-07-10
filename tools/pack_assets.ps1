# ============================================================
# pack_assets.ps1
#   Asset/ 以下の全ファイルを 1 つの assets.pak にまとめる。
#   Distribute 構成の PreBuildEvent から呼ばれ、生成された
#   assets.pak が Project.rc 経由で exe に埋め込まれる。
#
#   pak フォーマット（リトルエンディアン）:
#     'K''P''A''K'      : magic (4 byte)
#     uint32 version    : = 1
#     uint32 count      : ファイル数
#     [count 回]
#       uint32 pathLen  : 相対パスのバイト数(UTF-8)
#       byte[] path     : "Asset/Texture/x.png" のような相対パス(forward slash)
#       uint32 dataLen  : ファイルサイズ
#       byte[] data     : 中身
# ============================================================
$ErrorActionPreference = "Stop"

# プロジェクトルート = このスクリプトの 1 つ上(tools の親)
$root      = Split-Path -Parent $PSScriptRoot
$assetDir  = Join-Path $root "Asset"
$outPak    = Join-Path $root "assets.pak"

if (-not (Test-Path $assetDir)) {
    Write-Error "Asset folder not found: $assetDir"
    exit 1
}

# 実行時に書き換わるセーブ/生成ファイルと pak 自身は同梱しない
$excludeNames = @("save.dat", "settings.csv", "stage_records.csv", "totals.csv", "launched.flag")

# 同梱しないゴミ/ソース/バックアップ（.bak* / .blend* / .preedit 等）
$junkPattern = '\.(pak|bak|bak_.+|blend\d*|preedit)$'

# こもりBGMは廃止（ローパスで実現）→ 同梱しない
$muffledPattern = '_muffled\.mp3$'

$files = Get-ChildItem -Path $assetDir -Recurse -File | Where-Object {
    ($excludeNames -notcontains $_.Name) -and
    ($_.Name -notmatch $junkPattern) -and
    ($_.Name -notmatch $muffledPattern)
}

# まず平文 pak をファイルへ書き出す（FileStream は PS5.1/7 とも安定）
$fs = [System.IO.File]::Open($outPak, [System.IO.FileMode]::Create)
$bw = New-Object System.IO.BinaryWriter($fs)
try {
    $bw.Write([byte[]]@(0x4B, 0x50, 0x41, 0x4B))   # "KPAK"
    $bw.Write([uint32]1)                           # version
    $bw.Write([uint32]$files.Count)                # count

    # パスキーは CP932(Shift-JIS) で格納する。実行時の narrow 文字列リテラルが
    # MSVC により CP932 へ変換されるため、日本語名アセットでもキーが一致する。
    $enc932 = [System.Text.Encoding]::GetEncoding(932)

    foreach ($f in $files) {
        # root からの相対パス。区切りは forward slash に統一
        $rel = $f.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
        $pathBytes = $enc932.GetBytes($rel)
        $data      = [System.IO.File]::ReadAllBytes($f.FullName)

        $bw.Write([uint32]$pathBytes.Length)
        $bw.Write($pathBytes)
        $bw.Write([uint32]$data.Length)
        $bw.Write($data)
    }
}
finally {
    $bw.Flush(); $bw.Dispose(); $fs.Dispose()
}

# XOR 難読化（AssetCrypt.h と同一ロジック：key[i&7] xor ((i*31+7)&0xFF)）。
# 100MB超を PowerShell ループで回すと激遅なので、C# を Add-Type してネイティブ速度で処理。
Add-Type -TypeDefinition @'
public static class PakXor {
    public static void Apply(byte[] d) {
        byte[] K = { 0x5A, 0xC3, 0x91, 0x2E, 0x7F, 0xB4, 0x68, 0xD1 };
        for (long i = 0; i < d.LongLength; i++) {
            d[i] ^= (byte)(K[i & 7] ^ (byte)(i * 31 + 7));
        }
    }
}
'@
$bytes = [System.IO.File]::ReadAllBytes($outPak)
[PakXor]::Apply($bytes)
[System.IO.File]::WriteAllBytes($outPak, $bytes)

# MSBuild は assets.pak の変更を rc.exe の再実行トリガーと見なさない（Project.rc は
# 変わらないため）。pak を埋め込み直させるため Project.rc のタイムスタンプを更新する。
$rc = Join-Path $root "Src\Project.rc"
if (Test-Path $rc) { (Get-Item $rc).LastWriteTime = Get-Date }

$sizeMB = [math]::Round((Get-Item $outPak).Length / 1MB, 2)
Write-Host "[pack_assets] packed $($files.Count) files -> assets.pak ($sizeMB MB)"
