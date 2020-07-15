/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include "event.h"
#include "camera.h"

#include <atomic>
#include <memory>
#include <list>
#include <set>
#include <chrono>

class Node;
class Renderer;

class Engine
{
public:
  using GlobalEventCallback = std::function<void(Engine&, Event&)>;

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

  /**
   * The camera/viewport of the scene
   * TODO: For now this is a single camera managed by the engine. VR support will require something much more complicated, and multiple cameras in the scene would be handy.
   */
  Camera& camera();

  void addEvent(std::shared_ptr<Event> e);
  void addEvent(Event* e);
  void callEventCallbacks();

  void addGlobalEventCallback(GlobalEventCallback callback);

  const std::list<std::shared_ptr<Event>>& events() const;

  /**
   * Call to quit the rendering loop, shutdown the engine/renderer
   */
  void quit();

private:
  void initRenderer();
  void loop();
  void cleanup();

  std::unique_ptr<Renderer> mRend;
  std::shared_ptr<Node> mNodeGraph;

  std::chrono::time_point<std::chrono::high_resolution_clock> mTimeStart;
  std::chrono::time_point<std::chrono::high_resolution_clock> mTimeCurrent;

  Camera mCamera;

  std::list<std::shared_ptr<Event>> mEventQueue;
  std::list<GlobalEventCallback> mGlobalEventCallbacks;

  std::atomic<bool> mQuit;
};

#endif
