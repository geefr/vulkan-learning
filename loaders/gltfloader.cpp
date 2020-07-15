/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#include "gltfloader.h"
#include "meshnode.h"

#include "tiny_gltf.h"

#include <iostream>
#include <memory>

bool GLTFLoader::load(std::string fileName, Node& root) {

	// TODO: https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp
	// - Open the file, load into gltf - loadFromFile method
	// - Take the tinygltf::Node and iterate - loadNode method
	// - Extract information we can use
	//   - Ideally all of it, but we don't support animations/PBR yet
	

	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF gltfContext;

	bool binary = false;
	size_t extpos = fileName.rfind('.', fileName.length());
	if (extpos != std::string::npos) {
		binary = (fileName.substr(extpos + 1, fileName.length() - extpos) == "glb");
	}

	std::string error;
	std::string warning;
	bool fileLoaded = binary ? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, fileName.c_str()) : 
		                   gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, fileName.c_str());

	if( !fileLoaded ) {
		std::cerr << "Failed to load GLTF File: " << fileName << "\n";
		std::cerr << "Errors: " << error << "\n";
		std::cerr << "Warnings: " << warning << "\n";
		return false;
	}

	auto sceneID = gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0;
	auto& scene = gltfModel.scenes[sceneID];

	for( auto gltfNodeID = 0u; gltfNodeID < scene.nodes.size(); ++gltfNodeID ) {
		// Parse the gltf node and create one of ours
		auto& gltfNode = gltfModel.nodes[scene.nodes[gltfNodeID]];

		std::shared_ptr<Node> n(new MeshNode());
		blablabla you are here
	}
	
	return true;
}

