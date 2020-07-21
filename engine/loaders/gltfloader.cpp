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
  // Handy reference: https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp
  // Engine format is similar to what's used here - largely modelled on what's available in the glTF format,
  // which should be flexible enough for other (older) ones.
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

  if (!fileLoaded) {
    std::cerr << "Failed to load GLTF File: " << fileName << "\n";
    std::cerr << "Errors: " << error << "\n";
    std::cerr << "Warnings: " << warning << "\n";
    return {};
  }

  auto sceneID = gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0;
  auto& scene = gltfModel.scenes[sceneID];

  std::shared_ptr<Node> root(new Node());
  for (auto gltfNodeID = 0u; gltfNodeID < scene.nodes.size(); ++gltfNodeID) {
    // Parse the gltf node and create one of ours
    auto& gNode = gltfModel.nodes[scene.nodes[gltfNodeID]];

    auto materials = parseGltfMaterials(gltfModel);

    parseGltfNode(root, gNode, gltfModel, materials);
  }

  return root;
}

void GLTFLoader::parseGltfNode(std::shared_ptr<Node> targetParent, tinygltf::Node& gNode, tinygltf::Model& gModel, std::vector<std::shared_ptr<Material>> materials) {

  std::shared_ptr<Node> n(new Node());

  // Node's model matrix
  if (gNode.translation.size() == 3) n->translation() = glm::make_vec3(gNode.translation.data());
  if (gNode.rotation.size() == 4) {
    std::cerr << "TODO: GLTF Loader - Need node rotation from quat, ignoring rotation from gltf model" << std::endl;
  }
  if (gNode.scale.size() == 3) n->scale() = glm::make_vec3(gNode.scale.data());
  if (gNode.matrix.size() == 16) {
    std::cerr << "TODO: GLTF Loader - Setting matrix explicitly may conflict/add to separate translation/scale parameters?" << std::endl;
    n->userModelMatrix(glm::make_mat4x4(gNode.matrix.data()));
  }

  // Handle children of the gltf node
  for (auto& gChild : gNode.children) {
    parseGltfNode(n, gModel.nodes[gChild], gModel, materials);
  }

  // Parse mesh data
  // TODO: The engine/renderer is quite limited here, this will need rework later
  if (gNode.mesh > -1) {
    auto& gMesh = gModel.meshes[gNode.mesh];

    for (auto& gPrimitive : gMesh.primitives) {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      // Capture all the vertices used by the primitive
      // Looks like they're packed into a big buffer in the gltf model, with lookups from the primitive..cool!
      auto [posBuffer, posBufferCount, posBufferStride] = getFloatBuffer("POSITION", 3, gModel, gPrimitive);
      // Vertex position is the only mandatory attribute
      if (!posBuffer) continue;

      auto [normBuffer, normBufferCount, normBufferStride] = getFloatBuffer("NORMAL", 3, gModel, gPrimitive);
      auto [tex0Buffer, tex0BufferCount, tex0BufferStride] = getFloatBuffer("TEXCOORD_0", 2, gModel, gPrimitive);
      auto [tex1Buffer, tex1BufferCount, tex1BufferStride] = getFloatBuffer("TEXCOORD_1", 2, gModel, gPrimitive);
      // TODO - uint16_t! auto [posBuffer, posBufferCount, posBufferStride] = getFloatBuffer("JOINTS_0", 3, gModel, gPrimitive);
      // TODO - engine skinning support auto [weightBuffer, weightBufferCount, posBufferStride] = getFloatBuffer("WEGHTS_0", 3, gModel, gPrimitive);

      // Iterate through each vertex and translate to our format
      for (auto vI = 0u; vI < posBufferCount; ++vI) {
        Vertex vert;

        vert.position = glm::make_vec3(&posBuffer[vI * posBufferStride]);
        vert.normal = normBuffer ? glm::make_vec3(&normBuffer[vI * normBufferStride]) : glm::vec3(0.f,0.f,0.f);
        vert.uv0 = tex0Buffer ? glm::make_vec2(&tex0Buffer[vI * tex0BufferStride]) : glm::vec2(0.f,0.f);
        vert.uv1 = tex1Buffer ? glm::make_vec2(&tex1Buffer[vI * tex1BufferStride]) : glm::vec2(0.f,0.f);
        // TODO
        // - joints
        // - weights
        
        vertices.emplace_back(vert);
      }

      // The engine/renderer don't need indices, but will use them if present
      if (gPrimitive.indices > -1) {
        auto& indexAccess = gModel.accessors[gPrimitive.indices];
        auto& indexView = gModel.bufferViews[indexAccess.bufferView];
        auto& indexBuffer = gModel.buffers[indexView.buffer];
        auto indexPtr = reinterpret_cast<const void*>(&(indexBuffer.data[indexAccess.byteOffset + indexView.byteOffset]));

        for (auto indexI = 0u; indexI < indexAccess.count; ++indexI) {
          uint32_t index = 0;
          switch (indexAccess.componentType) {
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

      if (gPrimitive.material > -1 && static_cast<size_t>(gPrimitive.material) < materials.size()) {
        auto mat = materials[gPrimitive.material];
        mesh->material( mat );
      }

      n->children().emplace_back(mesh);
    }
  }

  // Add the new node to the root and finish
  targetParent->children().emplace_back(n);
}

std::tuple<const float*, size_t, size_t> GLTFLoader::getFloatBuffer(std::string attribute, uint32_t defaultSize, tinygltf::Model& gModel, tinygltf::Primitive& gPrimitive) {
  auto it = gPrimitive.attributes.find(attribute);
  if( it == gPrimitive.attributes.end() ) return {nullptr, 0, 0};
  const auto& access = gModel.accessors[it->second];
  const auto& view = gModel.bufferViews[access.bufferView];

  auto stride = access.ByteStride(view);
  if( stride == -1 ) {
    std::cerr << "WARNING: GLTFLoader: Failed to calculate stride of " << attribute << " buffer" << std::endl;
    return {nullptr, 0, 0};
  }
  if( stride > 0 ) stride /= sizeof(float);
  else stride = defaultSize * sizeof(float);

  auto buffer = reinterpret_cast<const float*>(&(gModel.buffers[view.buffer].data[access.byteOffset + view.byteOffset]));
  return {buffer, access.count, stride};
} 

std::vector<std::shared_ptr<Material>> GLTFLoader::parseGltfMaterials(tinygltf::Model& gModel) {
  std::vector<std::shared_ptr<Material>> mats;
  for( auto& gMat : gModel.materials ) {
    std::shared_ptr<Material> mat(new Material());
    // TODO: Texture support in engine
    /*auto baseColorTex = gMat.values.find("baseColorTexture");
    if( baseColorTex != gMat.values.end() ) {
      mat.baseColourTex
    }*/
    // metallicRoughnessTexture
    // normalTexture, mat.additionalValues
    // emissiveTexture, mat.additionalValues
    // occlusionTexture, mat.additionalValues

    auto roughnessFactor = gMat.values.find("roughnessFactor");
    if( roughnessFactor != gMat.values.end() ) mat->roughnessFactor = static_cast<float>(roughnessFactor->second.Factor());

    auto metallicFactor = gMat.values.find("metallicFactor");
    if( metallicFactor != gMat.values.end() ) mat->metallicFactor = static_cast<float>(metallicFactor->second.Factor());

    auto baseColorFactor = gMat.values.find("baseColorFactor");
    if( baseColorFactor != gMat.values.end() ) mat->baseColourFactor = glm::make_vec4(baseColorFactor->second.ColorFactor().data());

    auto alphaMode = gMat.additionalValues.find("alphaMode");
    if (alphaMode != gMat.additionalValues.end()) {
      const auto& alphaString = alphaMode->second.string_value;
      if(alphaString == "BLEND") mat->alphaMode = Material::AlphaMode::Blended;
      else if (alphaString == "MASK") {
        mat->alphaMode = Material::AlphaMode::Masked;
        mat->alphaCutOff = 0.5f;
      }
    }
    auto alphaCutoff = gMat.additionalValues.find("alphaCutoff");
    if( alphaCutoff != gMat.additionalValues.end() ) mat->alphaCutOff = static_cast<float>(alphaCutoff->second.Factor());

    auto emissiveFactor = gMat.additionalValues.find("emissiveFactor");
    if( emissiveFactor != gMat.additionalValues.end() ) mat->emissiveFactor = glm::vec4(glm::make_vec3(emissiveFactor->second.ColorFactor().data()), 1.0);

    // TODO: Yeah so as Sascha notes this bit is a little ugly, should sort out what we really need
    // Will really depend on sophistication of the shader pipeline as to what gets used there
    auto specGlossExtIt = gMat.extensions.find("KHR_materials_pbrSpecularGlossiness");
    if (specGlossExtIt != gMat.extensions.end()) {
      auto specGlossExt = specGlossExtIt->second;
      if (specGlossExt.Has("specularGlossinessTexture")) {
        // TODO: Reading of textures from GLTF
      }
      if (specGlossExt.Has("diffuseTexture")) {
        // TODO: Reading of textures from GLTF
      }
      if (specGlossExt.Has("diffuseFactor")) {
        auto factor = specGlossExt.Get("diffuseFactor");
        for (auto i = 0u; i < factor.ArrayLen(); ++i) {
          auto val = factor.Get(i);
          mat->diffuseFactor[i] = val.IsNumber() ? static_cast<float>(val.Get<double>()) : static_cast<float>(val.Get<int>());
        }
      }
      if (specGlossExt.Has("specularFactor")) {
        auto factor = specGlossExt.Get("specularFactor");
        for (auto i = 0u; i < factor.ArrayLen(); ++i) {
          auto val = factor.Get(i);
          mat->specularFactor[i] = val.IsNumber() ? static_cast<float>(val.Get<double>()) : static_cast<float>(val.Get<int>());
        }
      }
    }

    mats.emplace_back(mat);
  }

  return mats;
}
