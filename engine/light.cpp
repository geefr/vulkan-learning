#include "light.h"

Light::Light(std::string typeStr) {
  if( typeStr == "directional" ) mType = Type::Directional;
  else if( typeStr == "point" ) mType = Type::Point;
  else if( typeStr == "spot" ) mType = Type::Spot;
}

Light::Light(Type t)
  : mType(t)
{}

Light::~Light() {}

Light::Type Light::type() const { return mType; }
