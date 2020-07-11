/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include <memory>

class Node;
class Renderer;

class Engine
{
public:
  Engine();
  ~Engine();

  /** 
   * Start the engine
   * Call this once everything is setup
   * Once called don't modify anything (Initially all modifications to be done using node scripts/events?)
   */
  void run();

  /**
   * The root node of the graph
   * When rendering the engine will perform event/update/render traversals starting here.
   */
  std::shared_ptr<Node> nodegraph();

private:
  void initRenderer();
  void loop();
  void cleanup();

  std::unique_ptr<Renderer> mRend;
  std::shared_ptr<Node> mNodeGraph;
};

#endif
