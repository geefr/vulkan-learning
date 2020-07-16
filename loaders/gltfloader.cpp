/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#include "gltfloader.h"
#include "meshnode.h"


#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

std::shared_ptr<Node> GLTFLoader::load(std::string fileName) {

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
        return {};
    }

    auto sceneID = gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0;
    auto& scene = gltfModel.scenes[sceneID];

    std::shared_ptr<Node> root(new Node());
    for( auto gltfNodeID = 0u; gltfNodeID < scene.nodes.size(); ++gltfNodeID ) {
        // Parse the gltf node and create one of ours
        auto& gNode = gltfModel.nodes[scene.nodes[gltfNodeID]];

        parseGltfNode( root, gNode, gltfModel );
    }

    return root;
}

void GLTFLoader::parseGltfNode( std::shared_ptr<Node> targetParent, tinygltf::Node& gNode, tinygltf::Model& gModel ) {

    std::shared_ptr<Node> n(new Node());

    // Node's model matrix
    if( gNode.translation.size() == 3 ) n->translation() = glm::make_vec3(gNode.translation.data());
    if( gNode.rotation.size() == 4) {
        std::cerr << "TODO: GLTF Loader - Need node rotation from quat, ignoring rotation from gltf model" << std::endl;
    }
    if( gNode.scale.size() == 3 ) n->scale() = glm::make_vec3(gNode.scale.data());
    if( gNode.matrix.size() == 16 ) {
        std::cerr << "TODO: GLTF Loader - Setting matrix explicitly may conflict/add to separate translation/scale parameters?" << std::endl;
        n->userModelMatrix(glm::make_mat4x4(gNode.matrix.data()));
    }

    // Handle children of the gltf node
    for( auto& gChild : gNode.children ) {
        parseGltfNode( n, gModel.nodes[gChild], gModel );
    }

    // Parse mesh data
    // TODO: The engine/renderer is quite limited here, this will need rework later
    if( gNode.mesh > -1 ) {
        auto& gMesh = gModel.meshes[gNode.mesh];

        for( auto& gPrimitive : gMesh.primitives ) {
            /*
             * GLTF contains a lot of useful information - we can read just what we need here. Each vertex is similar to the usual vertex definition needed in shaders
             * Sascha's example reads the following, expect the format can contain more
             * - position (mandatory or we'll ignore it)
             * - normal
             * - texCoord 0 (TODO: Assume diffuse? Is there a way to tell from the gltf file?)
             * - texCoord 1 (TODO: Specular/Normal map? Is there a way to tell from the gltf file?)
             * - joints 0 (animation/rigging)
             * - weights 0 (animation/rigging)
             */
            if( gPrimitive.attributes.find("POSITION") == gPrimitive.attributes.end() ) continue;

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // Capture all the vertices used by the primitive
            // Looks like they're packed into a big buffer in the gltf model, with lookups from the primitive..cool!
            auto& positionAccess = gModel.accessors[gPrimitive.attributes.find("POSITION")->second];
            auto& positionView = gModel.bufferViews[positionAccess.bufferView];

            auto positionStride = positionAccess.ByteStride(positionView);
            // Convert stride from bytes -> floats
            if( positionStride == -1 ) {
                std::cerr << "WARNING: GLTFLoader: Failed to calculate stride of position view" << std::endl;
                continue;
            }
            if( positionStride > 0 ) positionStride = positionStride / sizeof(float);
            else positionStride = 3 * sizeof(float);

            auto positionBufferStart = reinterpret_cast<const float*>(&(gModel.buffers[positionView.buffer].data[positionAccess.byteOffset + positionView.byteOffset]));
            // Iterate through each vertex and translate to our format
            for( auto vI = 0u; vI < positionAccess.count; ++vI ) {
                Vertex vert;

                vert.position =glm::make_vec3(&positionBufferStart[vI * positionStride]);

                // TODO: Read the other vertex parameters, or at least whatever the renderer supports
                vertices.emplace_back(vert);
            }

            // The engine/renderer don't need indices, but will use them if present
            if( gPrimitive.indices > -1 ) {
                auto& indexAccess = gModel.accessors[gPrimitive.indices];
                auto& indexView = gModel.bufferViews[indexAccess.bufferView];
                auto& indexBuffer = gModel.buffers[indexView.buffer];
                auto indexPtr = reinterpret_cast<const void*>(&(indexBuffer.data[indexAccess.byteOffset + indexView.byteOffset]));

                for( auto indexI = 0u; indexI < indexAccess.count; ++indexI ) {
                    uint32_t index = 0;
                    switch( indexAccess.componentType ) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        index = reinterpret_cast<const uint32_t*>(indexPtr)[indexI];
                        break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        index = reinterpret_cast<const uint16_t*>(indexPtr)[indexI];
                        break;
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        index = reinterpret_cast<const uint8_t*>(indexPtr)[indexI];
                        break;
                    }
                    indices.emplace_back(index);
                }
            }

            std::shared_ptr<MeshNode> mesh(new MeshNode(vertices, indices));
            n->children().emplace_back(mesh);
        }
    }

    // Add the new node to the root and finish
    targetParent->children().emplace_back(n);
}
