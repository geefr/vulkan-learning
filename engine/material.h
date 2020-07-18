/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>

 /// Material definition, based on glTF concepts
 /// See Material struct - https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp
struct Material
{
  Material();
  ~Material();
  // TODO: Better docs
  enum class AlphaMode {
    Opaque,
    Masked,
    Blended,
  };
  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaCutOff = 1.f;
  float metallicFactor = 1.f;
  float roughnessFactor = 1.f;
  glm::vec4 baseColourFactor = { 1.f,1.f,1.f,1.f };
  glm::vec4 emissiveFactor = { 1.f,1.f,1.f,1.f };
  glm::vec4 diffuseFactor = { 1.f,1.f,1.f,1.f };
  glm::vec3 specularFactor = { 1.f,1.f,1.f };

  // Textures used by the material
  // TODO: Texture implementation
//     std::shared_ptr<Texture> baseColourTex;
//     std::shared_ptr<Texture> metallicRoughnessTex;
//     std::shared_ptr<Texture> normalTex;
//     std::shared_ptr<Texture> occlusionTex;
//     std::shared_ptr<Texture> emissiveTex;
//       std::shared_ptr<Texture> specularGlossinessTex;
//       std::shared_ptr<Texture> diffuseTex;

    // TODO: Not quite sure what this is
    // A lookup table for which textures should be used/Bound to each unit?
//    struct TexCoordSet {
//        uint8_t baseColour = 0;
//        uint8_t metallicRoughness = 0;
//        uint8_t specularGlossiness = 0;
//        uint8_t normal = 0;
//        uint8_t occlusion = 0;
//        uint8_t emissive = 0;
//    } texCoordSet;

    // TODO: Bools seem a strange choice here - Are these workflows exclusive or combined when it comes to the shader logic?
  bool pbrWorkflowMetallicRoughness = true;
  bool pbrWorkflowSpecularGlossiness = false;
};

#endif
