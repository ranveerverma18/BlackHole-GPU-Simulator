// ============================================
// A+ Black Hole Lensing Shader (3D Warp Hybrid)
// ============================================

uniform sampler2D tex;         // galaxy trail texture
uniform vec2      resolution;  // window size (pixels)
uniform vec2      center;      // black hole center in pixels

// ---- tunable parameters ----
uniform float lensStrength;        // radial lensing (1/r^2)
uniform float ringRadius;          // photon ring radius (in pixels)
uniform float ringWidth;           // photon ring thickness
uniform float ringBoost;           // photon ring brightness
uniform float dopplerBoost;        // one side brighter
uniform vec3  tint;                // global color tint

// ---- NEW: 3D warp params ----
uniform float verticalWarpStrength; // how high the disk "bends up"
uniform float verticalWarpFalloff;  // how fast warp falls off with radius
uniform float shearStrength;        // frame-drag shear (Kerr-like)
uniform float ringEccentricity;     // stretch photon ring vertically (oval)

void main() {
    // Screen-space coordinates (pixels)
    vec2 frag = gl_FragCoord.xy;
    vec2 toC  = frag - center;
    float r   = length(toC) + 0.0001;
    vec2 nd   = toC / r;   // normalized direction

    // Base UV = screen pixel
    vec2 uv = frag;

    // -----------------------------
    // 1. Radial Gravitational Lensing
    // -----------------------------
    float lens = lensStrength / (r * r + 20.0);
    uv -= nd * lens;

    // -----------------------------
    // 2. Frame-Drag Shear (Kerr-ish twist)
    // -----------------------------
    vec2 perp = vec2(-nd.y, nd.x);
    float shear = shearStrength / (r * r + 20000.0);
    uv += perp * shear;

    // -----------------------------
    // 3. Vertical Warp (3D elevation)
    // -----------------------------
    float angle = atan(toC.y, toC.x);  // -pi..pi
    float liftProfile = exp(-r / verticalWarpFalloff);
    float lift = verticalWarpStrength * liftProfile * sin(angle);
    uv.y += lift;

    // -----------------------------
    // FIX: Clamp UV so warp never samples outside texture
    // -----------------------------
    uv = clamp(uv, vec2(0.0), resolution - vec2(1.0));

    // Sample galaxy texture safely
    vec3 color = texture2D(tex, uv / resolution).rgb;

    // -----------------------------
    // Extra: Fade edges to hide clamps
    // -----------------------------
    float edgeFade = smoothstep(
        10.0, 40.0,
        min(frag.x, min(frag.y,
        min(resolution.x - frag.x, resolution.y - frag.y)))
    );
    color *= edgeFade;

    // ==========================
// 4. Photon Ring + Horizon
// ==========================

// --- Event horizon (clean solid circle)
float horizon = 0.0;
if (r < ringRadius * 0.35)
    horizon = 1.0;

// --- Photon ring (using RAW coords)
vec2 ringVecRaw = vec2(toC.x, toC.y * ringEccentricity);
float rRing = length(ringVecRaw);

float ring = exp(-abs(rRing - ringRadius) / ringWidth);

// Add ring only where appropriate
color += ring * ringBoost;

// Inner region = black
if (horizon > 0.5)
    color = vec3(0.0);



    // -----------------------------
    // 5. Doppler Beaming
    // -----------------------------
    float dop = cos(angle);  // +1 = right side, -1 = left side
    color *= (1.0 + dopplerBoost * dop);

    // -----------------------------
    // 6. Cinematic Tint
    // -----------------------------
    color *= tint;

    gl_FragColor = vec4(color, 1.0);
}


