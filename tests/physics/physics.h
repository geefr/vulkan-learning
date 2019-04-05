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
   *
   * vec4's are used here as packing rules mean vec3's would use 16 bytes anyway
   * To keep things simple here let's just maintain a 16-byte alignment for everything
   * and pad if we can't meet that
   */
  struct Particle {
    glm::vec4 position = {0,0,0,1};
    glm::vec4 velocity = {0,0,0,1};
    glm::vec4 force = {0,0,0,1};
    glm::vec4 colour = {1,1,1,1};
    float mass = 1; // Kg
    float radius = 1;
    float pad2;
    float pad3;
  };

  Physics() = delete;
  Physics( uint32_t numParticles );

  void logParticles();

  void step( float dT );

  std::vector<Particle>& particles() { return mParticles; }

private:
  void calcForce(Particle& p);
  void updatePosAndVel(Particle& p, float dT);
  void detectCollisions(float deltaT);
  void applyConstraints(float deltaT);

  std::vector<Particle> mParticles;

  glm::vec4 mFixedGravity = {0,-0.5,0,1};

  float mPointGravityMagnitude = 0.25;
  glm::vec4 mPointGravityLocation = {0,-90,0,1};
};

#endif // PHYSICS_H
