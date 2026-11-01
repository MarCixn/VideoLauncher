$file = "C:\Users\MarCixn\VideoLauncher\stm32_argb\ARGB_Controller_v2\ARGB_Controller_v2.ino"
$content = Get-Content $file -Raw

# Remove effectSparkle implementation
$sparkle = @"
void effectSparkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var) {
    uint8_t r, g, b;
    getVariantColor(var, &r, &g, &b);

    // Dim base color
    for (int i = 0; i < numLeds; i++) {
        strip->setPixelColor(i, strip->Color(r / 4, g / 4, b / 4));
    }

    // Random sparkles
    for (int i = 0; i < 3; i++) {
        int pos = random(numLeds);
        strip->setPixelColor(pos, strip->Color(255, 255, 255));
    }
}

"@
$content = $content.Replace($sparkle, "")
Write-Host "Removed effectSparkle"

# Remove remaining declarations (with different line endings)
$content = $content.Replace("void effectSparkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);", "")
$content = $content.Replace("void effectMeteor(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);", "")
$content = $content.Replace("void effectTwinkle(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);", "")
$content = $content.Replace("void effectStrobe(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);", "")
$content = $content.Replace("void effectHeartbeat(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);", "")
$content = $content.Replace("void effectBounce(Adafruit_NeoPixel* strip, int numLeds, ColorVariant var);", "")
Write-Host "Removed remaining declarations"

Set-Content -Path $file -Value $content -NoNewline
Write-Host "Part 2 done!"
