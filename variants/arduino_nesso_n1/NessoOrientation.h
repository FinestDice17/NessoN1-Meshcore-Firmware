#pragma once

// Auto-flip: detects a deliberate ~180-degree spin of the device about the
// axis pointing straight out of the screen (screen keeps facing the same
// way, content just rotates) using the onboard BMI270's gyroscope, and
// flips the display to match.
bool nessoOrientationBegin();
void nessoOrientationLoop();
