$file = "C:\Users\MarCixn\VideoLauncher\stm32_argb\ARGB_Controller_v2\ARGB_Controller_v2.ino"
$content = Get-Content $file -Raw

# 1. Update enum - remove unwanted programs
$oldEnum = @"
    PROG_SPARKLE,
    PROG_GRADIENT,
    PROG_TRICOLOR,    // Trzy kolory goni
"@
$newEnum = @"
    PROG_GRADIENT,
    PROG_TRICOLOR,    // Trzy kolory goni
"@
$content = $content.Replace($oldEnum, $newEnum)
Write-Host "1. Removed PROG_SPARKLE"

$oldEnum2 = @"
    PROG_METEOR,      // Kometa z ogonem
    PROG_THEATER,     // Theater chase
"@
$newEnum2 = @"
    PROG_THEATER,     // Theater chase
"@
$content = $content.Replace($oldEnum2, $newEnum2)
Write-Host "2. Removed PROG_METEOR"

$oldEnum3 = @"
    PROG_SCANNER,     // Knight Rider
    PROG_TWINKLE,     // Migaj
"@
$newEnum3 = @"
    PROG_SCANNER,     // Knight Rider z PWM
"@
$content = $content.Replace($oldEnum3, $newEnum3)
Write-Host "3. Removed PROG_TWINKLE"

$oldEnum4 = @"
    PROG_PLASMA,      // Plasma colors
    PROG_STROBE,      // Stroboskop
    PROG_CHASE,       // 5 LED grupki tam-powr
"@
$newEnum4 = @"
    PROG_PLASMA,      // Plasma colors
    PROG_CHASE,       // 5 LED grupki z PWM
"@
$content = $content.Replace($oldEnum4, $newEnum4)
Write-Host "4. Removed PROG_STROBE"

$oldEnum5 = @"
    PROG_HEARTBEAT,   // Bicie serca
    PROG_BOUNCE,      // Odbijaj
"@
$newEnum5 = ""
$content = $content.Replace($oldEnum5, $newEnum5)
Write-Host "5. Removed PROG_HEARTBEAT and PROG_BOUNCE"

# 2. Remove function declarations
$content = $content.Replace("void effectSparkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);`n", "")
$content = $content.Replace("void effectMeteor(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);`n", "")
$content = $content.Replace("void effectTwinkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);`n", "")
$content = $content.Replace("void effectStrobe(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);`n", "")
$content = $content.Replace("void effectHeartbeat(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);`n", "")
$content = $content.Replace("void effectBounce(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);`n", "")
Write-Host "6. Removed function declarations"

# 3. Remove case statements in applyEffect
$content = $content.Replace(@"
        case PROG_SPARKLE:
            effectSparkle(strip, numLeds, var);
            break;
"@, "")

$content = $content.Replace(@"
        case PROG_METEOR:
            effectMeteor(strip, numLeds, var);
            break;
"@, "")

$content = $content.Replace(@"
        case PROG_TWINKLE:
            effectTwinkle(strip, numLeds, var);
            break;
"@, "")

$content = $content.Replace(@"
        case PROG_STROBE:
            effectStrobe(strip, numLeds, var);
            break;
"@, "")

$content = $content.Replace(@"
        case PROG_HEARTBEAT:
            effectHeartbeat(strip, numLeds, var);
            break;
"@, "")

$content = $content.Replace(@"
        case PROG_BOUNCE:
            effectBounce(strip, numLeds, var);
            break;
"@, "")
Write-Host "7. Removed case statements"

Set-Content -Path $file -Value $content -NoNewline
Write-Host "Part 1 done!"
