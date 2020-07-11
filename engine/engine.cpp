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
{}

Engine::~Engine() {}

void Engine::run() {
	initRenderer();
	loop();
	cleanup();
}

std::shared_ptr<Node> Engine::nodegraph() { return mNodeGraph; }

void Engine::initRenderer() {
	mRend->initWindow();
	mRend->initVK();
}

void Engine::loop() {
	while(mRend->frame()) {}
}

void Engine::cleanup() {
	mRend->cleanup();
}

