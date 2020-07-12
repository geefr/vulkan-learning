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
	: mRend(new Renderer())
	, mNodeGraph(new Node())
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
		if( !mRend->pollWindowEvents() ) break;

		// Start the frame (Let the renderer reset what it needs)
		mRend->frameStart();

		// TODO: Update time delta
		mNodeGraph->update(0);

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

