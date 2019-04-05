#include "physics.h"

#include <random>
#include <iostream>

using glm::vec3;
using glm::vec4;

Physics::Physics(uint32_t numParticles) {
  std::random_device rd;
  std::mt19937 rdGen(rd());
  std::uniform_real_distribution<> dis(-10.0, 10.0);
  std::uniform_real_distribution<> disM(0.1, 10.0);
  std::uniform_real_distribution<> disC(0.0,1.0);
  for( auto i = 0u; i < numParticles; ++i ) {
    auto p = Particle();
    p.position = {dis(rdGen), 0.f /*dis(rdGen)*/, dis(rdGen), 1};
    p.mass = static_cast<float>(disM(rdGen));
    p.colour = {disC(rdGen), disC(rdGen), disC(rdGen), 1};

    p.velocity = vec4(0,10,0, 1);
    //p.velocity = vec3(dis(rdGen), dis(rdGen), dis(rdGen)) / 100.0f;

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

    // TODO: Need to fix their positions here, otherwise they'll slowly fall through the floor
    auto& pos = p.position;
    if( pos.x < -100 || pos.x > 100 ) {p.velocity *= vec4(-.9,0,0,1); }
    if( pos.y < -100 || pos.y > 100 ) {p.velocity *= vec4(0,-.9,0,1); }
    if( pos.z < -100 || pos.z > 100 ) {p.velocity *= vec4(0,0,-.9,1); }


  }

  detectCollisions(dT);
  applyConstraints(dT);
}

void Physics::calcForce(Particle& p) {
  p.force += mFixedGravity * p.mass;


  glm::vec4 pointGravVec = normalize(p.position - mPointGravityLocation);
  p.force += mPointGravityMagnitude * pointGravVec;


  // TODO: Apply some tiny random force just to avoid getting into obvious oscillations and such I guess
  // Right now there's no resistance/collisions/etc so the points will just clump up into groups with
  // floating point artifacts/etc (Of course we could use doubles for this)
  /*
  std::random_device rd;
  std::mt19937 rdGen(rd());
  std::uniform_real_distribution<> dis(-0.0001,0.0001);
  p.force += vec3(dis(rdGen), dis(rdGen), dis(rdGen));
*/


}

void Physics::updatePosAndVel(Particle& p, float dT) {
  vec4 a = p.force / p.mass;
  p.velocity += a * dT;
  // TODO:
  p.velocity.w = 1;
  p.position += p.velocity * dT;
}

void Physics::detectCollisions(float deltaT) {

}

void Physics::applyConstraints(float deltaT) {

}
