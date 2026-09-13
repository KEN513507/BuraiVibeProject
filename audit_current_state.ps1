param(
    [string]$Root = "C:\Users\user\Documents\game\BuraiVibeProject"
)

$ErrorActionPreference = "Continue"

function Section([string]$Title) {
    Write-Host ""
    Write-Host ("=" * 78) -ForegroundColor DarkGray
    Write-Host $Title -ForegroundColor Cyan
    Write-Host ("=" * 78) -ForegroundColor DarkGray
}

function Sub([string]$Title) {
    Write-Host ""
    Write-Host "--- $Title ---" -ForegroundColor Yellow
}

function ExistsReport([string]$RelativePath) {
    $p = Join-Path $Root $RelativePath
    if (Test-Path $p) {
        $item = Get-Item $p
        Write-Host ("[PASS] {0,-55} {1,10} bytes  {2}" -f `
            $RelativePath, $item.Length, $item.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")) `
            -ForegroundColor Green
        return $true
    }
    else {
        Write-Host "[MISS] $RelativePath" -ForegroundColor Red
        return $false
    }
}

function HashReport([string]$RelativePath) {
    $p = Join-Path $Root $RelativePath
    if (!(Test-Path $p)) {
        return $null
    }

    $item = Get-Item $p
    $hash = (Get-FileHash $p -Algorithm SHA256).Hash

    [PSCustomObject]@{
        Path      = $RelativePath
        Length    = $item.Length
        LastWrite = $item.LastWriteTime
        SHA256    = $hash
    }
}

function SearchFile {
    param(
        [string[]]$Paths,
        [string]$Pattern,
        [string]$Label
    )

    Sub $Label

    $valid = @()

    foreach ($path in $Paths) {
        $full = Join-Path $Root $path

        if (Test-Path $full) {
            $valid += $full
        }
    }

    if ($valid.Count -eq 0) {
        Write-Host "[NO FILES]" -ForegroundColor DarkYellow
        return
    }

    $hits = Select-String `
        -Path $valid `
        -Pattern $Pattern `
        -CaseSensitive:$false `
        -ErrorAction SilentlyContinue

    if ($hits) {
        foreach ($h in $hits) {
            $relative = $h.Path.Substring($Root.Length).TrimStart("\")
            Write-Host ("{0}:{1}: {2}" -f $relative, $h.LineNumber, $h.Line.Trim())
        }
    }
    else {
        Write-Host "[ZERO HITS]" -ForegroundColor DarkGray
    }
}

function Get-PngDimensions([string]$Path) {
    if (!(Test-Path $Path)) {
        return $null
    }

    try {
        $bytes = [System.IO.File]::ReadAllBytes($Path)

        if ($bytes.Length -lt 24) {
            return $null
        }

        # PNG signature
        $sig = @(137,80,78,71,13,10,26,10)

        for ($i = 0; $i -lt 8; $i++) {
            if ($bytes[$i] -ne $sig[$i]) {
                return $null
            }
        }

        $w = `
            ([int]$bytes[16] -shl 24) -bor `
            ([int]$bytes[17] -shl 16) -bor `
            ([int]$bytes[18] -shl 8)  -bor `
            ([int]$bytes[19])

        $h = `
            ([int]$bytes[20] -shl 24) -bor `
            ([int]$bytes[21] -shl 16) -bor `
            ([int]$bytes[22] -shl 8)  -bor `
            ([int]$bytes[23])

        return "$w`x$h"
    }
    catch {
        return $null
    }
}

Set-Location $Root

Section "BURAIVIBE CURRENT STATE AUDIT"
Write-Host "ROOT = $Root"
Write-Host "TIME = $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "MODE = READ ONLY"
Write-Host ""
Write-Host "NO FILE DELETE"
Write-Host "NO FILE MOVE"
Write-Host "NO SOURCE EDIT"
Write-Host "NO BUILD"
Write-Host "NO GAME LAUNCH"

# ----------------------------------------------------------------------
Section "1. PROJECT ROOT"

foreach ($f in @(
    "CMakeLists.txt",
    "src\main.cpp",
    "src\entity.h",
    "src\multi_part_entity.h",
    "src\multi_part_entity.cpp",
    "src\boss_entity.h",
    "src\boss_entity.cpp"
)) {
    ExistsReport $f | Out-Null
}

# ----------------------------------------------------------------------
Section "2. BOSS / MULTIPART ARCHITECTURE"

$bossFiles = @(
    "src\boss\part.h",
    "src\boss\single_part.h",
    "src\boss\composite_part.h",
    "src\boss\render_unit.h",
    "src\boss\oam.h",
    "src\boss\chain_node.h",
    "src\boss\joint.h",
    "src\boss\emitter.h",
    "src\boss\core.h",
    "src\boss\tip.h"
)

foreach ($f in $bossFiles) {
    ExistsReport $f | Out-Null
}

Sub "RigidMultiPartEntity"

foreach ($f in @(
    "src\rigid_multi_part_entity.h",
    "src\rigid_multi_part_entity.cpp"
)) {
    ExistsReport $f | Out-Null
}

SearchFile `
    -Paths @(
        "src\multi_part_entity.h",
        "src\multi_part_entity.cpp",
        "src\rigid_multi_part_entity.h",
        "src\rigid_multi_part_entity.cpp"
    ) `
    -Pattern "recompute_part_transforms|recompute_rigid_parts|root_chains|solve_fk" `
    -Label "Transform pipeline"

# ----------------------------------------------------------------------
Section "3. ORBITAL SHIELD ENEMY"

foreach ($f in @(
    "src\orbital_shield_enemy.h",
    "src\orbital_shield_enemy.cpp"
)) {
    ExistsReport $f | Out-Null
}

SearchFile `
    -Paths @(
        "src\orbital_shield_enemy.h",
        "src\orbital_shield_enemy.cpp"
    ) `
    -Pattern "SHIELD_COUNT|UpdateChase|update_chase|UpdateOrbit|update_orbit|RecomputeShield|recompute_rigid|resolve_player_bullet|ResolvePlayerBullet|CullIfOffscreen|cull_if_offscreen" `
    -Label "Orbital implementation symbols"

# ----------------------------------------------------------------------
Section "4. FOUR-BALL CONTRACT AUDIT"

$orbitalPaths = @(
    "src\orbital_shield_enemy.h",
    "src\orbital_shield_enemy.cpp",
    "tests\orbital_shield_tests.cpp"
)

SearchFile `
    -Paths $orbitalPaths `
    -Pattern "SHIELD_COUNT\s*=\s*3|array<[^>]+,\s*3>|120|2\s*\*\s*.*PI\s*/\s*3|4\s*\*\s*.*PI\s*/\s*3" `
    -Label "Expected 1 Core + 3 Shields evidence"

SearchFile `
    -Paths $orbitalPaths `
    -Pattern "SHIELD_COUNT\s*=\s*4|array<[^>]+,\s*4>|parts\.size\(\)\s*==\s*5|entries\.size\(\)\s*==\s*5|four shields|4 shields|5[- ]?ball|5 balls|90.degree|PI\s*\*\s*0\.5" `
    -Label "STALE 5-PART / 4-SHIELD evidence"

# ----------------------------------------------------------------------
Section "5. OLD SDL / PNG DEPENDENCY AUDIT"

SearchFile `
    -Paths @(
        "src\orbital_shield_enemy.h",
        "src\orbital_shield_enemy.cpp"
    ) `
    -Pattern "SDL_RenderCopy|SDL_RenderFillRect|SDL_Color|IMG_Load|SpriteSheetStrip|LoadStrip|LoadOrbitalShieldSprites|DestroyOrbitalShieldSprites|filesystem|enemy_orbital_.*\.png" `
    -Label "Old direct SDL / PNG dependencies"

# ----------------------------------------------------------------------
Section "6. OAM PIPELINE"

SearchFile `
    -Paths @(
        "src\boss\render_unit.h",
        "src\boss\oam.h",
        "src\boss\single_part.h",
        "src\boss\composite_part.h",
        "src\boss\oam_sdl_renderer.h",
        "src\boss\oam_sdl_renderer.cpp"
    ) `
    -Pattern "RenderPrimitive|PlaceholderRect|primitive|width|height|palette_id|emit_to|resolve_flicker|MAX_SPRITES|SCANLINE_LIMIT|SDL_RenderFillRect" `
    -Label "OAM / Placeholder implementation"

# ----------------------------------------------------------------------
Section "7. MAIN.CPP INTEGRATION"

SearchFile `
    -Paths @("src\main.cpp") `
    -Pattern "OrbitalShield|RigidMultiPart|boss::OAM|resolve_flicker|RenderOAM|render_oam|OAM" `
    -Label "Orbital/OAM references in main.cpp"

SearchFile `
    -Paths @("src\main.cpp") `
    -Pattern "hazardChains|UpdateRotatingChain|RenderHazardChains" `
    -Label "Existing stage hazard chain references"

# ----------------------------------------------------------------------
Section "8. CMAKE INTEGRATION"

Write-Host ""
Get-Content (Join-Path $Root "CMakeLists.txt") |
    Select-String `
        -Pattern "add_executable|orbital|rigid_multi|multi_part|boss_entity|enemy\.cpp|oam_sdl|Tests|tests/" `
        -CaseSensitive:$false |
    ForEach-Object {
        Write-Host ("CMakeLists.txt:{0}: {1}" -f $_.LineNumber, $_.Line.Trim())
    }

# ----------------------------------------------------------------------
Section "9. TEST FILES"

foreach ($f in @(
    "tests\orbital_shield_tests.cpp",
    "tests\enemy_tests.cpp",
    "tests\player_asset_validator_test.py",
    "tests\player_sprite_contract_tests.cpp"
)) {
    ExistsReport $f | Out-Null
}

if (Test-Path ".\tests\orbital_shield_tests.cpp") {
    SearchFile `
        -Paths @("tests\orbital_shield_tests.cpp") `
        -Pattern "parts\.size|entries\.size|SHIELD_COUNT|120|90|BLOCKED_BY_SHIELD|CORE_DAMAGED|CORE_DESTROYED|active|orbit" `
        -Label "Orbital test expectations"
}

# ----------------------------------------------------------------------
Section "10. REFERENCE IMAGES"

$references = @(
    "_archive\reference_images\ChatGPT Image 2026年9月13日 22_54_42.png",
    "_archive\reference_images\5a9ed65e-46b7-4ed0-8a72-6ed214b50542.png"
)

foreach ($r in $references) {
    $full = Join-Path $Root $r

    if (Test-Path $full) {
        $item = Get-Item $full
        $dim = Get-PngDimensions $full

        Write-Host ("[PASS] {0}" -f $r) -ForegroundColor Green
        Write-Host ("       SIZE_BYTES = {0}" -f $item.Length)
        Write-Host ("       DIMENSIONS = {0}" -f $(if ($dim) { $dim } else { "UNKNOWN" }))
        Write-Host ("       REFERENCE_ONLY = YES")
    }
    else {
        Write-Host "[MISS] $r" -ForegroundColor Red
    }
}

# ----------------------------------------------------------------------
Section "11. PLAYER PIPELINE STATUS"

foreach ($f in @(
    "assets\sprites\player\player.json",
    "tools\player_asset_validator.py",
    "src\player_sprite_contract.h",
    "src\player_sprite_contract.cpp"
)) {
    ExistsReport $f | Out-Null
}

Sub "Canonical player PNG count"

$playerDir = Join-Path $Root "assets\sprites\player"

if (Test-Path $playerDir) {
    $canonical = Get-ChildItem $playerDir -File -Filter "player_*.png" -ErrorAction SilentlyContinue
    Write-Host "PLAYER_CANONICAL_PNG_COUNT = $($canonical.Count)"

    foreach ($p in $canonical) {
        $dim = Get-PngDimensions $p.FullName
        Write-Host ("  {0,-28} {1}" -f $p.Name, $dim)
    }
}

# ----------------------------------------------------------------------
Section "12. ZERO-BYTE FILE AUDIT"

$zeroFiles = Get-ChildItem $Root -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $_.Length -eq 0 -and
        $_.FullName -notmatch "\\build\\CMakeFiles\\"
    }

if ($zeroFiles) {
    foreach ($z in $zeroFiles) {
        $rel = $z.FullName.Substring($Root.Length).TrimStart("\")
        Write-Host "[ZERO] $rel" -ForegroundColor DarkYellow
    }
}
else {
    Write-Host "ZERO_BYTE_FILES = NONE" -ForegroundColor Green
}

# ----------------------------------------------------------------------
Section "13. EXTERNAL / CONCURRENT WRITER SNAPSHOT"

$watched = @(
    "src\main.cpp",
    "CMakeLists.txt",
    "src\orbital_shield_enemy.h",
    "src\orbital_shield_enemy.cpp",
    "src\multi_part_entity.h",
    "src\multi_part_entity.cpp"
)

$before = @{}

foreach ($f in $watched) {
    $s = HashReport $f
    if ($s) {
        $before[$f] = $s
        Write-Host ("BEFORE {0}" -f $f)
        Write-Host ("  LastWrite = {0}" -f $s.LastWrite.ToString("yyyy-MM-dd HH:mm:ss.fff"))
        Write-Host ("  SHA256    = {0}" -f $s.SHA256)
    }
}

Write-Host ""
Write-Host "Waiting 3 seconds to detect active external writes..." -ForegroundColor DarkGray
Start-Sleep -Seconds 3

$changed = @()

foreach ($f in $watched) {
    $after = HashReport $f

    if ($after -and $before.ContainsKey($f)) {
        if ($after.SHA256 -ne $before[$f].SHA256) {
            $changed += $f
            Write-Host "[CHANGED DURING AUDIT] $f" -ForegroundColor Red
        }
    }
}

if ($changed.Count -eq 0) {
    Write-Host "CONCURRENT_WRITER_GATE = PASS (no watched file changed during 3-second window)" -ForegroundColor Green
}
else {
    Write-Host "CONCURRENT_WRITER_GATE = BLOCKED" -ForegroundColor Red
}

# ----------------------------------------------------------------------
Section "14. BUILD ARTIFACT STATUS - READ ONLY"

$exeCandidates = Get-ChildItem (Join-Path $Root "build") `
    -Recurse `
    -File `
    -Filter "*.exe" `
    -ErrorAction SilentlyContinue

if ($exeCandidates) {
    foreach ($exe in $exeCandidates) {
        $rel = $exe.FullName.Substring($Root.Length).TrimStart("\")
        Write-Host ("EXE {0,-60} {1}" -f $rel, $exe.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss"))
    }
}
else {
    Write-Host "NO EXE FOUND"
}

Write-Host ""
Write-Host "NOTE: executable was NOT launched."

# ----------------------------------------------------------------------
Section "15. HIGH-LEVEL STATUS SUMMARY"

$hasRigidH = Test-Path (Join-Path $Root "src\rigid_multi_part_entity.h")
$hasRigidCpp = Test-Path (Join-Path $Root "src\rigid_multi_part_entity.cpp")
$hasOrbital = Test-Path (Join-Path $Root "src\orbital_shield_enemy.cpp")
$hasOamBridge = Test-Path (Join-Path $Root "src\boss\oam_sdl_renderer.cpp")
$hasOrbitalTests = Test-Path (Join-Path $Root "tests\orbital_shield_tests.cpp")

Write-Host ("RIGID_MULTI_PART_ENTITY = {0}" -f $(if ($hasRigidH -and $hasRigidCpp) {"PRESENT"} else {"MISSING/PARTIAL"}))
Write-Host ("ORBITAL_SOURCE          = {0}" -f $(if ($hasOrbital) {"PRESENT"} else {"MISSING"}))
Write-Host ("OAM_SDL_BRIDGE          = {0}" -f $(if ($hasOamBridge) {"PRESENT"} else {"MISSING"}))
Write-Host ("ORBITAL_TESTS           = {0}" -f $(if ($hasOrbitalTests) {"PRESENT"} else {"MISSING"}))

Write-Host ""
Write-Host "EXPECTED FINAL SPEC:" -ForegroundColor Cyan
Write-Host "  OrbitalShieldEnemy = 1 Core + 3 Shields"
Write-Host "  TOTAL OBJECTS       = 4"
Write-Host "  Shield spacing      = 120 degrees"
Write-Host "  Shield collision    = invincible / bullet block"
Write-Host "  Core collision      = damageable"
Write-Host "  Render              = Part -> OAM -> SDL bridge"
Write-Host "  Reference PNG       = reference only, NO resize"

Section "AUDIT COMPLETE"

Write-Host "No project file was intentionally edited by this script." -ForegroundColor Green