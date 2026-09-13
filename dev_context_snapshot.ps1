param(
    [string]$Root = "C:\Users\user\Documents\game\BuraiVibeProject"
)

$ErrorActionPreference = "Continue"

# PowerShellの日本語表示をUTF-8へ寄せる
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)

function Section([string]$title) {
    Write-Host ""
    Write-Host ("=" * 80) -ForegroundColor DarkGray
    Write-Host $title -ForegroundColor Cyan
    Write-Host ("=" * 80) -ForegroundColor DarkGray
}

function Sub([string]$title) {
    Write-Host ""
    Write-Host "--- $title ---" -ForegroundColor Yellow
}

function Rel([string]$path) {
    if ($path.StartsWith($Root)) {
        return $path.Substring($Root.Length).TrimStart("\")
    }
    return $path
}

function FileInfo([string]$rel) {
    $p = Join-Path $Root $rel
    if (Test-Path $p) {
        $x = Get-Item $p
        Write-Host ("[PASS] {0,-55} {1,9} bytes  {2}" -f `
            $rel, $x.Length, $x.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss.fff")) `
            -ForegroundColor Green
    } else {
        Write-Host "[MISS] $rel" -ForegroundColor Red
    }
}

function Grep {
    param(
        [string[]]$Files,
        [string]$Pattern,
        [string]$Label
    )

    Sub $Label
    $paths = @()

    foreach ($f in $Files) {
        $p = Join-Path $Root $f
        if (Test-Path $p) { $paths += $p }
    }

    if ($paths.Count -eq 0) {
        Write-Host "[NO FILES]"
        return
    }

    $hits = Select-String `
        -Path $paths `
        -Pattern $Pattern `
        -CaseSensitive:$false `
        -Encoding UTF8 `
        -ErrorAction SilentlyContinue

    if (!$hits) {
        Write-Host "[ZERO HITS]" -ForegroundColor DarkGray
        return
    }

    foreach ($h in $hits) {
        Write-Host ("{0}:{1}: {2}" -f (Rel $h.Path), $h.LineNumber, $h.Line.Trim())
    }
}

function Sha([string]$rel) {
    $p = Join-Path $Root $rel
    if (!(Test-Path $p)) { return $null }

    [PSCustomObject]@{
        Path      = $rel
        Hash      = (Get-FileHash $p -Algorithm SHA256).Hash
        LastWrite = (Get-Item $p).LastWriteTime
    }
}

function PngSize([string]$path) {
    try {
        $b = [System.IO.File]::ReadAllBytes($path)
        if ($b.Length -lt 24) { return "INVALID" }

        if (
            $b[0] -ne 137 -or $b[1] -ne 80 -or
            $b[2] -ne 78  -or $b[3] -ne 71
        ) { return "NOT_PNG" }

        $w = ([int]$b[16] -shl 24) -bor
             ([int]$b[17] -shl 16) -bor
             ([int]$b[18] -shl 8)  -bor
             ([int]$b[19])

        $h = ([int]$b[20] -shl 24) -bor
             ([int]$b[21] -shl 16) -bor
             ([int]$b[22] -shl 8)  -bor
             ([int]$b[23])

        return "${w}x${h}"
    }
    catch {
        return "ERROR"
    }
}

Set-Location $Root

Section "BURAIVIBE DEVELOPMENT CONTEXT SNAPSHOT"

Write-Host "ROOT      = $Root"
Write-Host "TIME      = $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "MODE      = READ ONLY"
Write-Host "BUILD     = NO"
Write-Host "SOURCE EDIT = NO"
Write-Host "GIT WRITE = NO"

# ============================================================
Section "1. GIT LOCAL STATE"

Write-Host "BRANCH:"
git branch --show-current

Write-Host "`nHEAD:"
git log -1 --oneline

Write-Host "`nREMOTE:"
git remote -v

Write-Host "`nSTATUS:"
git status --short

Write-Host "`nCHANGED FILES:"
git diff --name-status

Write-Host "`nDIFF STAT:"
git diff --stat

Sub "Recent local commits"
git log --oneline -5

# ============================================================
Section "2. CURRENT HOT FILES"

$hot = @(
    "src\main.cpp",
    "src\orbital_enemy_field.h",
    "src\orbital_enemy_field.cpp",
    "src\orbital_shield_enemy.h",
    "src\orbital_shield_enemy.cpp",
    "src\rigid_multi_part_entity.h",
    "src\multi_part_entity.cpp",
    "src\oam_sdl_bridge.h",
    "src\oam_sdl_bridge.cpp",
    "src\boss\render_unit.h",
    "src\boss\oam.h",
    "tests\orbital_shield_tests.cpp",
    "CMakeLists.txt"
)

foreach ($f in $hot) {
    FileInfo $f
}

# ============================================================
Section "3. ORBITAL PLAYER COLLISION"

Grep `
    -Files @(
        "src\orbital_enemy_field.h",
        "src\orbital_enemy_field.cpp",
        "src\orbital_shield_enemy.h",
        "src\orbital_shield_enemy.cpp",
        "src\main.cpp"
    ) `
    -Pattern "HitsPlayer|hits_player|Player.*collision|overlap.*player|coreHitbox|shieldHitbox" `
    -Label "Player vs Orbital API / implementation"

Grep `
    -Files @("src\main.cpp") `
    -Pattern "damaged|HitsPlayer|orbitalEnemies|debugInvincible" `
    -Label "main.cpp damaged chain"

# ============================================================
Section "4. PLAYER BULLET VS ORBITAL REGRESSION"

Grep `
    -Files @(
        "src\main.cpp",
        "src\orbital_enemy_field.cpp",
        "src\orbital_shield_enemy.cpp"
    ) `
    -Pattern "ResolvePlayerBullet|resolve_player_bullet|BLOCKED_BY_SHIELD|CORE_DAMAGED|CORE_DESTROYED" `
    -Label "Bullet collision path"

# ============================================================
Section "5. FOUR-BALL CONTRACT"

Grep `
    -Files @(
        "src\orbital_shield_enemy.h",
        "src\orbital_shield_enemy.cpp",
        "tests\orbital_shield_tests.cpp"
    ) `
    -Pattern "SHIELD_COUNT|PART_COUNT|120|parts\.size|entries\.size" `
    -Label "Expected Core 1 + Shield 3"

Grep `
    -Files @(
        "src\orbital_shield_enemy.h",
        "src\orbital_shield_enemy.cpp",
        "tests\orbital_shield_tests.cpp"
    ) `
    -Pattern "SHIELD_COUNT\s*=\s*4|parts\.size\(\)\s*==\s*5|entries\.size\(\)\s*==\s*5|4 shields|four shields|5 balls|5-ball" `
    -Label "STALE old specification"

# ============================================================
Section "6. ORBITAL REAL-ASSET READINESS"

Sub "Potential Orbital assets"

$assetRoots = @(
    "assets\sprites",
    "assets\raw_enemies"
)

foreach ($ar in $assetRoots) {
    $p = Join-Path $Root $ar
    if (!(Test-Path $p)) { continue }

    Get-ChildItem $p -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -match "orbital|shield|core"
        } |
        ForEach-Object {
            $dim = ""
            if ($_.Extension -ieq ".png") {
                $dim = PngSize $_.FullName
            }

            Write-Host ("{0,-70} {1,10} bytes  {2}" -f `
                (Rel $_.FullName), $_.Length, $dim)
        }
}

Sub "Reference images"

$refs = @(
    "_archive\reference_images\ChatGPT Image 2026年9月13日 22_54_42.png",
    "_archive\reference_images\5a9ed65e-46b7-4ed0-8a72-6ed214b50542.png"
)

foreach ($r in $refs) {
    $p = Join-Path $Root $r
    if (Test-Path $p) {
        Write-Host ("[PASS] {0}  {1}  REFERENCE_ONLY" -f `
            $r, (PngSize $p)) -ForegroundColor Green
    } else {
        Write-Host "[MISS] $r" -ForegroundColor Red
    }
}

# ============================================================
Section "7. IMAGE LOADER / TILE RENDER PATH"

Grep `
    -Files @(
        "src\oam_sdl_bridge.h",
        "src\oam_sdl_bridge.cpp",
        "src\boss\render_unit.h",
        "src\boss\oam.h",
        "src\orbital_shield_enemy.cpp",
        "src\player_sprite.cpp",
        "src\stage.cpp"
    ) `
    -Pattern "IMG_Load|SDL_CreateTexture|SDL_RenderCopy|SDL_RenderCopyEx|tile_id|PLACEHOLDER_TILE_ID|placeholder|texture" `
    -Label "Texture / placeholder path"

Sub "Direct conclusion helpers"

$bridge = Join-Path $Root "src\oam_sdl_bridge.cpp"

if (Test-Path $bridge) {
    $txt = Get-Content $bridge -Raw -Encoding UTF8

    Write-Host ("OAM_HAS_RENDER_COPY = {0}" -f `
        $(if ($txt -match "SDL_RenderCopy") {"YES"} else {"NO"}))

    Write-Host ("OAM_HAS_FILL_RECT   = {0}" -f `
        $(if ($txt -match "SDL_RenderFillRect") {"YES"} else {"NO"}))

    Write-Host ("OAM_LOADS_TEXTURE   = {0}" -f `
        $(if ($txt -match "IMG_Load|SDL_CreateTexture") {"YES"} else {"NO"}))
}

# ============================================================
Section "8. SOUND TEST MERGE STATUS"

Grep `
    -Files @("src\main.cpp") `
    -Pattern "SOUND_TEST|SoundTest|sound test|SDLK_F[0-9]|PlaySE_|bgm|APU" `
    -Label "Sound test / audio additions"

# ============================================================
Section "9. MAIN RESTORE / BACKUP RISK"

Sub "main.cpp backups"

Get-ChildItem $Root -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $_.Name -match "^main\.cpp.*bak|main.*backup"
    } |
    ForEach-Object {
        Write-Host ("{0}  {1}" -f `
            (Rel $_.FullName),
            $_.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss.fff"))
    }

Sub "main.cpp current identity"

$mainSha = Sha "src\main.cpp"
if ($mainSha) {
    Write-Host "SHA256    = $($mainSha.Hash)"
    Write-Host "LASTWRITE = $($mainSha.LastWrite.ToString('yyyy-MM-dd HH:mm:ss.fff'))"
}

# ============================================================
Section "10. TEST EXPECTATIONS"

Grep `
    -Files @("tests\orbital_shield_tests.cpp") `
    -Pattern "HitsPlayer|Shield|Core|OAM|entries|collision|active|destroyed|orbit|120" `
    -Label "Orbital test coverage"

# ============================================================
Section "11. BUILD BINARY FRESHNESS"

$exeList = @(
    "build\BuraiVibeGame.exe",
    "build\OrbitalShieldTests.exe"
)

$latestSource = Get-ChildItem `
    (Join-Path $Root "src"), `
    (Join-Path $Root "tests") `
    -Recurse -File `
    -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

Write-Host "LATEST_SOURCE = $((Rel $latestSource.FullName))"
Write-Host "SOURCE_TIME   = $($latestSource.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss.fff'))"

foreach ($e in $exeList) {
    $p = Join-Path $Root $e

    if (!(Test-Path $p)) {
        Write-Host "[MISS] $e" -ForegroundColor Red
        continue
    }

    $x = Get-Item $p
    $fresh = $x.LastWriteTime -ge $latestSource.LastWriteTime

    Write-Host ("{0,-35} {1}  FRESH={2}" -f `
        $e,
        $x.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss.fff"),
        $fresh)
}

# ============================================================
Section "12. CONCURRENT WRITER GATE"

$watch = @(
    "src\main.cpp",
    "src\orbital_enemy_field.h",
    "src\orbital_enemy_field.cpp",
    "src\orbital_shield_enemy.cpp",
    "tests\orbital_shield_tests.cpp",
    "CMakeLists.txt"
)

$before = @{}

foreach ($f in $watch) {
    $s = Sha $f
    if ($s) { $before[$f] = $s.Hash }
}

Write-Host "Waiting 5 seconds..." -ForegroundColor DarkGray
Start-Sleep -Seconds 5

$changed = @()

foreach ($f in $watch) {
    $s = Sha $f
    if (
        $s -and
        $before.ContainsKey($f) -and
        $before[$f] -ne $s.Hash
    ) {
        $changed += $f
        Write-Host "[CHANGED DURING AUDIT] $f" -ForegroundColor Red
    }
}

if ($changed.Count -eq 0) {
    Write-Host "CONCURRENT_WRITER_GATE = PASS"
} else {
    Write-Host "CONCURRENT_WRITER_GATE = BLOCKED" -ForegroundColor Red
}

# ============================================================
Section "13. MOST USEFUL LOCAL DIFF"

foreach ($f in @(
    "src/main.cpp",
    "src/orbital_enemy_field.h",
    "src/orbital_enemy_field.cpp",
    "src/orbital_shield_enemy.cpp",
    "tests/orbital_shield_tests.cpp"
)) {
    Write-Host ""
    Write-Host ">>> git diff -- $f" -ForegroundColor Yellow

    git diff --unified=3 -- $f |
        Select-Object -First 160
}

# ============================================================
Section "14. SNAPSHOT SUMMARY"

Write-Host "EXPECTED CURRENT TARGET:"
Write-Host "  OrbitalShieldEnemy = Core 1 + Shield 3"
Write-Host "  Player bullet -> Shield/Core = REQUIRED"
Write-Host "  Player body -> Shield/Core   = REQUIRED"
Write-Host "  Placeholder                 = TEMPORARY"
Write-Host "  Real Orbital sprite         = NOT ASSUMED"
Write-Host "  Reference resize            = FORBIDDEN"
Write-Host ""
Write-Host "CHECK THESE FIRST:"
Write-Host "  1. Git local diff"
Write-Host "  2. HitsPlayer exists and main.cpp calls it"
Write-Host "  3. OrbitalShieldTests contains Player-body tests"
Write-Host "  4. EXE freshness"
Write-Host "  5. OAM still placeholder-only or real texture path exists"
Write-Host "  6. Concurrent writer gate"

Section "SNAPSHOT COMPLETE"
Write-Host "No source/build/Git data was modified."