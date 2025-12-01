#include <SFML/Graphics.hpp>
#include <cmath>
#include <bits/stdc++.h>
using namespace std;
int main() {
    const unsigned WINDOW_W = 1280;
    const unsigned WINDOW_H = 720;

    sf::RenderWindow window(
        sf::VideoMode(WINDOW_W, WINDOW_H),
        "GPU Black Hole Raytracer - Phase G1",
        sf::Style::Close
    );
    window.setFramerateLimit(60);

    sf::Shader bhShader;
    if (!bhShader.loadFromFile("bh_raymarch.frag", sf::Shader::Fragment)) {
        return 1;
    }

    // Fullscreen quad
    sf::RectangleShape screen(sf::Vector2f(WINDOW_W, WINDOW_H));
    screen.setPosition(0.f, 0.f);

    sf::Clock clock;

    // Camera setup
    sf::Glsl::Vec3 camPos(0.f, 1.0f, 12.0f);   // above + in front
    sf::Glsl::Vec3 camTarget(0.f, 0.f, 0.f);  // look near origin
    float fovDegrees = 55.0f;
    float fovFactor = std::tan(fovDegrees * 3.14159265f / 360.0f); // tan(FOV/2)

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed)
                window.close();
            if (e.type == sf::Event::KeyPressed &&
                e.key.code == sf::Keyboard::Escape)
                window.close();
        }

        float time = clock.getElapsedTime().asSeconds();

        // Set uniforms
        bhShader.setUniform("uResolution", sf::Glsl::Vec2(WINDOW_W, WINDOW_H));
        bhShader.setUniform("uTime", time);

        bhShader.setUniform("uCamPos", camPos);
        bhShader.setUniform("uCamTarget", camTarget);
        bhShader.setUniform("uFovFactor", fovFactor);

        // Scene params
        bhShader.setUniform("uBhRadius", 3.0f);
        bhShader.setUniform("uDiskInner", 4.0f);
        bhShader.setUniform("uDiskOuter", 10.0f);
        bhShader.setUniform("uDiskHeight", 0.5f);
        bhShader.setUniform("uDiskRotation", 0.5f); // spin speed

        // PHASE G2: static tilt ~30 degrees
        float tiltDegrees = 27.0f;
        float tiltRadians = tiltDegrees * 3.14159265f / 180.0f;
        bhShader.setUniform("uDiskTilt", tiltRadians);
        
        //PHASE G3:ray bending controls
        bhShader.setUniform("uGravStrength", 0.8f);      // bending strength
        bhShader.setUniform("uStepSize", 0.10f);        // quality vs performance
        bhShader.setUniform("uDiskColorBase", sf::Glsl::Vec3(1.2f, 0.9f, 1.4f));

        window.clear(sf::Color::Black);
        window.draw(screen, &bhShader);
        window.display();
    }

    return 0;
}
