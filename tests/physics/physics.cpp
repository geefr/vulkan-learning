#include "physics.h"

#include <random>
#include <iostream>

using glm::vec3;

Physics::Physics(uint32_t numParticles) {
  std::random_device rd;
  std::mt19937 rdGen(rd());
  std::uniform_real_distribution<> dis(-100.0, 100.0);
  std::uniform_real_distribution<> disM(0.1, 10.0);
  for( auto i = 0u; i < numParticles; ++i ) {
    auto p = Particle();
    p.position = {dis(rdGen), 0.0/*dis(rdGen)*/, dis(rdGen)};
    p.mass = static_cast<float>(disM(rdGen));
    mParticles.emplace_back(p);
  }
}

void Physics::logParticles() {
  auto i = 0u;
  for(auto& p : mParticles) {
    std::cout << "Particle " << i++ << " " << p.mass << "Kg (" << p.position.x << "," << p.position.y << "," << p.position.z << ")\n";;
  }
  std::cout << std::endl;
}

void Physics::step(float dT) {

  for( auto & p : mParticles ) {
    calcForce(p);
    updatePosAndVel(p, dT);
  }

  detectCollisions(dT);
  applyConstraints(dT);
}

void Physics::calcForce(Particle& p) {
  p.force += mGravity * p.mass;
}

void Physics::updatePosAndVel(Particle& p, float dT) {
  vec3 a = p.force / p.mass;
  p.velocity += a * dT;
  p.position += p.velocity * dT;
}

void Physics::detectCollisions(float deltaT) {

}

void Physics::applyConstraints(float deltaT) {

}
