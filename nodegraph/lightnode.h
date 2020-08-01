/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef LIGHTNODE_H
#define LIGHTNODE_H

#include "node.h"

#include "renderer.h"
#include "light.h"

/// A node which renders a light
class LightNode final : public Node
{
public:
  LightNode();
  LightNode(const Light& l);
  virtual ~LightNode();

  // Render this node and any children
  void doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat) override;

  Light& light();
private:
  Light mLight;
};

#endif
