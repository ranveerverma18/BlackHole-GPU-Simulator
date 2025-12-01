#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
using namespace std;

// ----------------------
// Simple random helper
// ----------------------
float randFloat(float a, float b) {
    static mt19937 rng{ std::random_device{}() };
    uniform_real_distribution<float> dist(a, b);
    return dist(rng);
}

// ----------------------
// Simulation parameters
// ----------------------
struct SimParams {
    float G         = 2.0f;     // grav. constant (sim units)
    float M_bh      = 400.0f;   // black hole mass
    float softening = 0.5f;     // avoids infinite accel at center

    float v0        = 2.2f;     // dark matter flat-curve velocity
    float r_core    = 1.2f;     // halo core radius

    float dt        = 0.01f;    // time step

    // ------- PHASE 3: Accretion Disk Parameters -------
    float viscosityBase  = 0.003f;   // base friction strength
    float viscosityCore  = 1.0f;     // prevents infinite viscosity near r → 0
    float heatScale      = 0.0012f;  // controls brightness generation
    float brightnessCool = 0.997f;   // glow cool-down factor
    float horizonRadius  = 7.0f;     // event horizon swallow radius
    float respawnRMin    = 18.0f;    // outer disk respawn range
    float respawnRMax    = 28.0f;
};

// ----------------------
// Galaxy simulation (SoA)
// ----------------------
struct GalaxySim {
    SimParams P;

    vector<float> posX;
    vector<float> posY;
    vector<float> velX;
    vector<float> velY;
    vector<float> brightness;

    // ---------------------------------------------
    // Respawn particle when it falls into the BH
    // ---------------------------------------------
    void respawnAtOuterRing(int i) {
        float u = randFloat(0.0f, 1.0f);
        float r = P.respawnRMin + (P.respawnRMax - P.respawnRMin) * u;
        float theta = randFloat(0.0f, 2.0f * 3.14159265f);

        float x = r * cos(theta);
        float y = r * sin(theta);

        posX[i] = x;
        posY[i] = y;

        float dist = max(sqrt(x * x + y * y), 0.1f);
        float rx = x / dist;
        float ry = y / dist;
        float tx = -ry;
        float ty =  rx;

        float v_bh = sqrt(P.G * P.M_bh / (dist + P.softening)); 
        float v_dm = P.v0;
        float v_circ = sqrt(v_bh * v_bh + v_dm * v_dm);

        float jitter = randFloat(-0.05f, 0.05f);
        float v = v_circ * 1.4f * (1.0f + jitter);

        velX[i] = tx * v;
        velY[i] = ty * v;

        brightness[i] = 0.6f; 
    }

    void init(int count) {
        posX.resize(count);
        posY.resize(count);
        velX.resize(count);
        velY.resize(count);
        brightness.resize(count);

        const float R_MIN = 2.0f;
        const float R_MAX = 30.0f;

        for (int i = 0; i < count; ++i) {
            float u = randFloat(0.0f, 1.0f);
            float r = R_MIN + (R_MAX - R_MIN) * sqrt(u);
            float theta = randFloat(0.0f, 2.0f * 3.14159265f);

            float x = r * cos(theta);
            float y = r * sin(theta);
            posX[i] = x;
            posY[i] = y;

            float dist = std::max(sqrt(x * x + y * y), 0.1f);
            float rx = x / dist;
            float ry = y / dist;
            float tx = -ry;
            float ty =  rx;

            float v_bh = sqrt(P.G * P.M_bh / (dist + P.softening));
            float v_dm = P.v0;
            float v_circ = sqrt(v_bh * v_bh + v_dm * v_dm);

            float jitter = randFloat(-0.05f, 0.05f);
            float v = v_circ * 1.6f * (1.0f + jitter);

            velX[i] = tx * v;
            velY[i] = ty * v;

            brightness[i] = randFloat(0.5f, 1.0f);
        }
    }

    void step() {
        const int n = static_cast<int>(posX.size());
        for (int i = 0; i < n; ++i) {
            float x = posX[i];
            float y = posY[i];

            float dist = sqrt(x * x + y * y) + 1e-3f;
            float invDist = 1.0f / dist;

            // direction towards center
            float dx = -x * invDist;
            float dy = -y * invDist;

            // black hole acceleration
            float a_bh_mag = P.G * P.M_bh / (dist * dist + P.softening);
            float a_bh_x = dx * a_bh_mag;
            float a_bh_y = dy * a_bh_mag;

            // dark matter halo: flat rotation
            float a_dm_mag = (P.v0 * P.v0) / (dist + P.r_core);
            float a_dm_x = dx * a_dm_mag;
            float a_dm_y = dy * a_dm_mag;

            float ax = a_bh_x + a_dm_x;
            float ay = a_bh_y + a_dm_y;

            velX[i] += ax * P.dt;
            velY[i] += ay * P.dt;

            posX[i] += velX[i] * P.dt;
            posY[i] += velY[i] * P.dt;

            // --------------------------------------------------
            // ------- PHASE 3: ACCRETION DISK PHYSICS -----------
            // --------------------------------------------------

            float eta = P.viscosityBase / (dist + P.viscosityCore);
            if (eta > 0.02f) eta = 0.02f;

            float vx = velX[i];
            float vy = velY[i];
            float speed2 = vx*vx + vy*vy;

            float heat = P.heatScale * eta * speed2;
            brightness[i] += heat;

            if (brightness[i] > 2.0f) brightness[i] = 2.0f;

            float damp = 1.0f - eta;
            velX[i] *= damp;
            velY[i] *= damp;

            brightness[i] *= P.brightnessCool;
            if (brightness[i] < 0.2f) brightness[i] = 0.2f;

            if (dist < P.horizonRadius) {
                respawnAtOuterRing(i);
            }
        }
    }
};

// ----------------------
// Color based on speed + brightness
// ----------------------
sf::Color starColor(float vx, float vy, float bright) {
    float speed = sqrt(vx * vx + vy * vy);
    float t = clamp(speed / 6.0f, 0.0f, 1.0f);

    float glow = min(bright, 2.0f);

    sf::Uint8 r = static_cast<sf::Uint8>(220 * glow);
    sf::Uint8 g = static_cast<sf::Uint8>(140 * glow);
    sf::Uint8 b = static_cast<sf::Uint8>(80  * glow + 60 * t);

    return sf::Color(r, g, b);
}

// ----------------------
// Main
// ----------------------
int main() {
    const unsigned WINDOW_W = 1280;
    const unsigned WINDOW_H = 720;

    sf::RenderWindow window(
        sf::VideoMode(WINDOW_W, WINDOW_H),
        "Black Hole + Dark Matter Halo Galaxy",
        sf::Style::Close
    );
    window.setFramerateLimit(60);

    sf::Vector2f centerScreen(WINDOW_W / 2.f, WINDOW_H / 2.f);
    float scale = 12.0f;

    GalaxySim sim;
    const int NUM_STARS = 10000;
    sim.init(NUM_STARS);

    // ---- Phase 2: render texture for trails ----
    sf::RenderTexture trailRT;
    if (!trailRT.create(WINDOW_W, WINDOW_H)) return 1;
    trailRT.clear(sf::Color(0, 0, 10));
    trailRT.display();

    sf::VertexArray starVertices(sf::Points, NUM_STARS);

    // rectangle to gently fade old pixels (trail effect)
    sf::RectangleShape fadeRect(sf::Vector2f(WINDOW_W, WINDOW_H));
    fadeRect.setFillColor(sf::Color(0, 0, 10, 20));

    // ---- PHASE 4: Load lensing shader ----
    sf::Shader lensShader;
    lensShader.loadFromFile("lensing.frag", sf::Shader::Fragment);

    sf::Clock clock;

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed)
                window.close();

            if (e.type == sf::Event::KeyPressed) {
                if (e.key.code == sf::Keyboard::Escape)
                    window.close();
                if (e.key.code == sf::Keyboard::R)
                    sim.init(NUM_STARS);  // reseed galaxy
            }
        }

        (void)clock.restart();

        // ---- Phase 1: update simulation ----
        sim.step();

        // ---- Phase 2: update vertices ----
        for (int i = 0; i < NUM_STARS; ++i) {
            float x = sim.posX[i];
            float y = sim.posY[i];

            sf::Vector2f screenPos(
                centerScreen.x + x * scale,
                centerScreen.y + y * scale
            );

            starVertices[i].position = screenPos;
            starVertices[i].color =
                starColor(sim.velX[i], sim.velY[i], sim.brightness[i]);
        }

        // ---- Draw into trailRT ----
        trailRT.setView(trailRT.getDefaultView());

        trailRT.draw(fadeRect, sf::BlendAlpha);
        trailRT.draw(starVertices, sf::BlendAdd);


        trailRT.display();

        // ---- PHASE 4: Apply lensing shader ----
                // ---- PHASE 4: Apply lensing shader ----
        lensShader.setUniform("tex", trailRT.getTexture());
        lensShader.setUniform("resolution", sf::Vector2f(WINDOW_W, WINDOW_H));
        lensShader.setUniform("center", centerScreen);

        // A+ enhanced values (with 3D warp)
        lensShader.setUniform("lensStrength", 18000.0f);          // radial lensing
        lensShader.setUniform("ringRadius", 110.0f);              // photon ring radius (in pixels)
        lensShader.setUniform("ringWidth", 3.0f);                 // ring thickness
        lensShader.setUniform("ringBoost", 3.5f);                 // how bright the ring is
        lensShader.setUniform("dopplerBoost", 0.7f);              // stronger asymmetry
        lensShader.setUniform("tint", sf::Glsl::Vec3(1.1f, 1.05f, 0.95f));

        // PHASE 5: 3D disk warping controls
        lensShader.setUniform("verticalWarpStrength", 40.0f);     // how high the disk bends
        lensShader.setUniform("verticalWarpFalloff", 260.0f);     // larger = bend farther out
        lensShader.setUniform("shearStrength", 9000.0f);          // twisting around BH
        lensShader.setUniform("ringEccentricity", 1.4f);          // >1 = taller photon ring


        // ---- Present on window ----
        window.clear(sf::Color::Black);

        sf::Sprite finalImage(trailRT.getTexture());
        window.draw(finalImage, &lensShader);

        window.display();
    }

    return 0;
}




