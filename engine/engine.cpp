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

	loop();
	cleanup();
}

std::shared_ptr<Node> Engine::nodegraph() { return mNodeGraph; }

void Engine::initRenderer() {
	mRend->initWindow();
	mRend->initVK();
}

void Engine::loop() {
	while(true) {
		// Poll for events
		// TODO: Just handling the window here - should poll any
		// other input devices/systems like joysticks, VR controllers, etc
		mEventQueue.clear();
		// Renderer can request a quit here - TODO: Should really handle as a QuitEvent instance
	        if( !mRend->pollWindowEvents() ) break;
		// Handle explicit event callbacks (Not bound to any particular node/script)
		callEventCallbacks();

		// Renderer or other events may have asked us to stop rendering
		if( mQuit ) break;

		// Start the frame (Let the renderer reset what it needs)
		mRend->frameStart();

		// TODO: Update time delta - Should do this at the start of the loop, have the time available for any global event callbacks?
		mNodeGraph->update(*this, 0);

		// TODO: Proper values for matrices - Need to add cameras
		glm::mat4x4 viewMat(1.0);
		glm::mat4x4 projMat(1.0);
		
		mNodeGraph->render(*mRend.get(), viewMat, projMat);

		// Finish the frame, renderer sends commands to gpu here
		mRend->frameEnd();
	}
}

void Engine::cleanup() {
	mRend->waitIdle();
	mNodeGraph->cleanup(*mRend.get());
	mRend->cleanup();
}

void Engine::addEvent(std::shared_ptr<Event> e) { mEventQueue.emplace_back(e); }
void Engine::addEvent(Event* e) { mEventQueue.emplace_back(e); }

void Engine::addGlobalEventCallback(Engine::GlobalEventCallback callback) {
	mGlobalEventCallbacks.emplace_back(callback);
}

void Engine::callEventCallbacks() {
	for( auto& e : mEventQueue ) {
		for( auto& c : mGlobalEventCallbacks ) {
			c(*this, *e.get());
		}
	}
}

const std::list<std::shared_ptr<Event>>& Engine::events() const { return mEventQueue; }

void Engine::quit() { mQuit = true; }


