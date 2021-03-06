/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#include "engine.h"
#include "renderer.h"
#include "node.h"

Engine::Engine()
  : mRend(new Renderer(*this))
  , mNodeGraph(new Node())
  , mQuit(false)
{}

Engine::~Engine() {}

void Engine::run() {
  initRenderer();

  mNodeGraph->init(*mRend.get());
  mNodeGraph->upload(*mRend.get());

  mTimeStart = std::chrono::high_resolution_clock::now();
  mTimeCurrent = mTimeStart;

  loop();
  cleanup();
}

std::shared_ptr<Node> Engine::nodegraph() { return mNodeGraph; }

void Engine::initRenderer() {
  mRend->initWindow();
  mRend->initVK();
}

void Engine::loop() {
  while (true) {
    try {
      // Calculate the frame time
      std::chrono::time_point<std::chrono::high_resolution_clock> current = std::chrono::high_resolution_clock::now();
      auto deltaT = ((double)(current - mTimeCurrent).count()) / 1.0e9;
      mTimeCurrent = current;

      // Poll for events
      // TODO: Just handling the window here - should poll any
      // other input devices/systems like joysticks, VR controllers, etc
      mEventQueue.clear();
      // Renderer can request a quit here - TODO: Should really handle as a QuitEvent instance
      if (!mRend->pollWindowEvents()) break;

      // Handle explicit event callbacks (Not bound to any particular node/script)
      callEventCallbacks();

      // Renderer or other events may have asked us to stop rendering
      if (mQuit) break;

      // Perform the update traversal
      mNodeGraph->update(*this, deltaT);

      // Start the frame (Let the renderer reset what it needs)
      mRend->frameStart();

      // Render the scene
      mNodeGraph->render(*mRend.get(), mCamera.mViewMatrix, mCamera.mProjectionMatrix);

      // Finish the frame, renderer sends commands to gpu here
      mRend->frameEnd();
    }
    catch (...) {
      // Something went wrong, we don't know what but
      // it's critical that we unwind the renderer's resources properly
      cleanup();
      // And just forward the error to whoever is listening
      throw;
    }
  }
}

void Engine::cleanup() {
  mRend->waitIdle();
  mNodeGraph->cleanup(*mRend.get());
  mRend->cleanup();
}

Camera& Engine::camera() { return mCamera; }
float Engine::windowWidth() const { return static_cast<float>(mRend->windowWidth()); }
float Engine::windowHeight() const { return static_cast<float>(mRend->windowHeight()); }

void Engine::addEvent(std::shared_ptr<Event> e) { mEventQueue.emplace_back(e); }
void Engine::addEvent(Event* e) { mEventQueue.emplace_back(e); }

void Engine::addGlobalEventCallback(Engine::GlobalEventCallback callback) {
  mGlobalEventCallbacks.emplace_back(callback);
}

void Engine::callEventCallbacks() {
  for (auto& e : mEventQueue) {
    for (auto& c : mGlobalEventCallbacks) {
      c(*this, *e.get());
    }
  }
}

const std::list<std::shared_ptr<Event>>& Engine::events() const { return mEventQueue; }

void Engine::quit() { mQuit = true; }


