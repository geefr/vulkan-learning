#ifndef PHYSICS_H
#define PHYSICS_H

#include "glm/glm.hpp"

#include <vector>

class Physics
{
public:
  /**
   * A Particle
   * All units are in meters/SI units
   */
  struct Particle {
    glm::vec3 position = {0,0,0};
    glm::vec3 velocity = {0,0,0};
    float mass = 1; // Kg
    glm::vec3 force = {0,0,0};
  };

  Physics() = delete;
  Physics( uint32_t numParticles );

  void logParticles();

  void step( float dT );



private:
  void calcForce(Particle& p);
  void updatePosAndVel(Particle& p, float dT);
  void detectCollisions(float deltaT);
  void applyConstraints(float deltaT);

  std::vector<Particle> mParticles;

  glm::vec3 mGravity = {0,-0.01,0};
};

#endif // PHYSICS_H
