#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

// ----------------------
// Simple random helper
// ----------------------
float randFloat(float a, float b) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> dist(a, b);
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
};

// ----------------------
// Galaxy simulation (SoA)
// ----------------------
struct GalaxySim {
    SimParams P;

    std::vector<float> posX;
    std::vector<float> posY;
    std::vector<float> velX;
    std::vector<float> velY;
    std::vector<float> brightness;

    void init(int count) {
        posX.resize(count);
        posY.resize(count);
        velX.resize(count);
        velY.resize(count);
        brightness.resize(count);

        const float R_MIN = 2.0f;
        const float R_MAX = 30.0f;

        for (int i = 0; i < count; ++i) {
            // Radius (biased for denser core)
            float u = randFloat(0.0f, 1.0f);
            float r = R_MIN + (R_MAX - R_MIN) * std::sqrt(u);
            float theta = randFloat(0.0f, 2.0f * 3.14159265f);

            float x = r * std::cos(theta);
            float y = r * std::sin(theta);

            posX[i] = x;
            posY[i] = y;

            float dist = std::max(std::sqrt(x * x + y * y), 0.1f);

            // radial + tangential directions
            float rx = x / dist;
            float ry = y / dist;
            // tangent is rotated (-y, x)
            float tx = -ry;
            float ty =  rx;

            // approximate circular velocity (BH + halo)
            float v_bh = std::sqrt(P.G * P.M_bh / (dist + P.softening));
            float v_dm = P.v0;
            float v_circ = std::sqrt(v_bh * v_bh + v_dm * v_dm);

            // more aggressive disc spin, tiny jitter → spiral structure
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

            float dist = std::sqrt(x * x + y * y) + 1e-3f;
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

            // semi-implicit Euler
            velX[i] += ax * P.dt;
            velY[i] += ay * P.dt;

            posX[i] += velX[i] * P.dt;
            posY[i] += velY[i] * P.dt;
        }
    }
};

// ----------------------
// Color based on speed + brightness
// ----------------------
sf::Color starColor(float vx, float vy, float bright) {
    float speed = std::sqrt(vx * vx + vy * vy);
    float t = std::clamp(speed / 6.0f, 0.0f, 1.0f); // tune 6.0f if needed

    // interpolate: inner fast → blue/white, outer slow → pinkish
    sf::Uint8 r = static_cast<sf::Uint8>(220 * (0.5f + 0.5f * t));
    sf::Uint8 g = static_cast<sf::Uint8>(180 * (0.8f - 0.3f * t));
    sf::Uint8 b = static_cast<sf::Uint8>(255 * (1.0f - 0.2f * t) + 40 * t);

    r = static_cast<sf::Uint8>(r * bright);
    g = static_cast<sf::Uint8>(g * bright);
    b = static_cast<sf::Uint8>(b * bright);

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
    float scale = 12.0f; // sim units → pixels

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
    fadeRect.setFillColor(sf::Color(0, 0, 10, 20)); // alpha small → longer trails

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

        // fixed physics step
        (void)clock.restart();

        // ---- Phase 1: update simulation ----
        sim.step();

        // ---- Phase 2: update vertices for batch draw ----
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

        // ---- Draw into trail render texture ----
        trailRT.setView(trailRT.getDefaultView());

        // fade old frame slightly to create motion blur trails
        trailRT.draw(fadeRect, sf::BlendAlpha);

        // draw all stars in one call
        trailRT.draw(starVertices, sf::BlendAdd);

        // draw central black hole glow on the render texture
        {
            float bhOuter = 18.0f;
            float bhInner = 9.0f;

            sf::CircleShape glow(bhOuter);
            glow.setOrigin(bhOuter, bhOuter);
            glow.setPosition(centerScreen);
            glow.setFillColor(sf::Color(255, 170, 60, 210));
            trailRT.draw(glow, sf::BlendAdd);

            sf::CircleShape hole(bhInner);
            hole.setOrigin(bhInner, bhInner);
            hole.setPosition(centerScreen);
            hole.setFillColor(sf::Color(0, 0, 0, 255));
            trailRT.draw(hole);
        }

        trailRT.display();

        // ---- Present on window ----
        window.clear(sf::Color::Black);

        sf::Sprite galaxy(trailRT.getTexture());
        window.draw(galaxy);

        window.display();
    }

    return 0;
}


