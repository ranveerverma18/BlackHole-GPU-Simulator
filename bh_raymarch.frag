// ============================================
// Phase G3 - GPU Black Hole + Tilted Disk
// Ray-marched with simple GR-like bending
// ============================================

uniform vec2  uResolution;     // window size in pixels
uniform float uTime;           // seconds since start

// Camera
uniform vec3  uCamPos;
uniform vec3  uCamTarget;
uniform float uFovFactor;

// Scene
uniform float uBhRadius;
uniform float uDiskInner;
uniform float uDiskOuter;
uniform float uDiskRotation;

// Tilt (same as before)
uniform float uDiskTilt;

// NEW: simple "gravity strength" + step size for bending
uniform float uGravStrength;
uniform float uStepSize;

// Colors
uniform vec3  uDiskColorBase;

// ----------------------
// Build camera ray
// ----------------------
vec3 makeRayDirection(vec2 fragCoord) {
    vec2 ndc = (2.0 * fragCoord - uResolution) / uResolution.y;

    vec3 forward = normalize(uCamTarget - uCamPos);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 right   = normalize(cross(forward, worldUp));
    vec3 up      = cross(right, forward);

    vec3 dir = normalize(
        forward +
        ndc.x * uFovFactor * right +
        ndc.y * uFovFactor * up
    );
    return dir;
}

void main() {
    vec2 frag = gl_FragCoord.xy;

    // 1) Camera ray
    vec3 ro = uCamPos;
    vec3 rd0 = makeRayDirection(frag);

    // 2) Disk plane normal (tilt around X-axis)
    float c = cos(uDiskTilt);
    float s = sin(uDiskTilt);
    vec3 diskNormal = normalize(vec3(0.0, c, s));

    // Build disk local basis (diskX, diskZ) spanning the plane
    vec3 tmpAxis = vec3(1.0, 0.0, 0.0);
    if (abs(dot(tmpAxis, diskNormal)) > 0.99) {
        tmpAxis = vec3(0.0, 0.0, 1.0);
    }
    vec3 diskX = normalize(cross(tmpAxis, diskNormal));
    vec3 diskZ = cross(diskNormal, diskX);

    // 3) Ray-march with simple gravity
    vec3 pos = ro;
    vec3 dir = rd0;

    bool hitBH   = false;
    bool hitDisk = false;

    vec3 diskHitPos = vec3(0.0);
    float diskR     = 0.0;
    float diskXc    = 0.0;
    float diskZc    = 0.0;

    float maxDist = 80.0;
    float stepSize = uStepSize;

    // Signed distance to disk plane at current pos
    float dPlane = dot(pos, diskNormal);

    const int MAX_STEPS = 140;
    for (int i = 0; i < MAX_STEPS; ++i) {
        float r = length(pos);

        // Black hole capture
        if (r < uBhRadius) {
            hitBH = true;
            break;
        }

        // Early exit if we flew far away
        if (r > maxDist) {
            break;
        }

        // Check if we cross the disk plane in this step
        vec3 nextPos = pos + dir * stepSize;
        float dNext = dot(nextPos, diskNormal);

        if ((dPlane > 0.0 && dNext <= 0.0) || (dPlane < 0.0 && dNext >= 0.0)) {
            // We crossed the plane between pos and nextPos
            float frac = dPlane / (dPlane - dNext); // 0..1
            vec3 hp = pos + dir * (stepSize * frac);

            float x = dot(hp, diskX);
            float z = dot(hp, diskZ);
            float rDisk = length(vec2(x, z));

            if (rDisk > uDiskInner && rDisk < uDiskOuter) {
                hitDisk   = true;
                diskHitPos = hp;
                diskR      = rDisk;
                diskXc     = x;
                diskZc     = z;
                break;
            }
        }

        // Simple GR-like bending: acceleration towards center ~ 1/r^2
        float eps  = 0.05;
        float invR = 1.0 / max(r, eps);
        float grav = uGravStrength * invR * invR;    // ~ 1/r^2
        vec3 acc   = -pos * grav * invR;             // toward origin

        // Update direction & position
        dir = normalize(dir + acc * stepSize);
        pos = nextPos;

        dPlane = dNext;
    }

    vec3 color = vec3(0.0); // background / hole

    // ====== DISK SHADING (if hit) ======
    if (hitDisk) {
        // diskHitPos, diskR, diskXc, diskZc are already computed
        float angle = atan(diskZc, diskXc);
        float spin  = angle + uDiskRotation * uTime;

        float tRad = (diskR - uDiskInner) / (uDiskOuter - uDiskInner);
        float radialBright = 1.5 - tRad * 1.2;
        radialBright = clamp(radialBright, 0.0, 1.0);

        // Tangent direction in the disk plane
        vec3 tangent = normalize(-diskZc * diskX + diskXc * diskZ);

        float doppler     = dot(-dir, tangent);
        float dopplerBoost = 1.0 + 0.8 * doppler;

        float brightness = radialBright * dopplerBoost;

        float band = 0.3 + 0.7 * sin(spin * 4.0);
        brightness *= 0.6 + 0.4 * band;

        color = uDiskColorBase * brightness;
    }
    // ====== BLACK HOLE SHADOW ======
    else if (hitBH) {
        color = vec3(0.0);
    }
    // else: stays background black

    gl_FragColor = vec4(color, 1.0);
}


