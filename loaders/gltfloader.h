/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef GLTFLOADER_H
#define GLTFLOADER_H

#include <string>
#include <memory>

class Node;
namespace tinygltf {
  class Node;
  class Model;
  class Primitive;
}

/// TODO: For now just a nasty static utility method,
/// loaders should be a proper class/concept once the
/// engine can support lazy resource loading in some
/// background thread, or some generic scene format
/// that includes multiple input formats.
class GLTFLoader {
public:
  static std::shared_ptr<Node> load(std::string fileName);
private:
  static void parseGltfNode( std::shared_ptr<Node> targetParent, tinygltf::Node& gNode, tinygltf::Model& gModel );
  /// @return <pointer to start of buffer, num elements in buffer, buffer stride per element>
  static std::tuple<const float*, size_t, size_t> getFloatBuffer(std::string attribute, uint32_t defaultSize, 
                                                      tinygltf::Model& gModel, tinygltf::Primitive& gPrimitive);

};

#endif

