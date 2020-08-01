#include "lightnode.h"

LightNode::LightNode()
  : mLight(Light::Type::Point)
{
}

LightNode::~LightNode()
{
}

LightNode::LightNode(const Light& l)
  : mLight(l)
{
}

void LightNode::doRender(Renderer& rend, mat4x4 nodeMat, mat4x4, mat4x4)
{
  glm::vec4 pos(0.0f);
  pos = nodeMat * pos; // To world space
  mLight.position = pos;
  mLight.direction = pos;
  rend.renderLight(mLight);
}

Light& LightNode::light() { return mLight; }

