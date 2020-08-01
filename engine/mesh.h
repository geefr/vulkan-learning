/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef MESH_H
#define MESH_H

#include "vertex.h"

#include "util/simplebuffer.h"

#include <vector>
#include <memory>

class Renderer;

// A mesh - A block of data to be rendered
struct Mesh {
    Mesh(const std::vector<Vertex>& v, const std::vector<uint32_t>& i);

    /// Create buffers and upload data to the GPU
    void upload(Renderer& rend);

    /// Delete buffers
    void cleanup(Renderer& rend);

    /// @return true if the mesh is setup for rendering
    bool validForRender() const;

    // (Will be cleared once uploaded)
    std::vector<Vertex> mVertices;
    std::vector<uint32_t> mIndices;

    // TODO: Directly referencing vulkan-specific stuff here, would need to change with alternate Renderer implementation
    std::unique_ptr<SimpleBuffer> mVertexBuffer;
    std::unique_ptr<SimpleBuffer> mIndexBuffer;
    // TODO: Store command buffer on a per-mesh basis, or recreate each frame? What's faster?
    // TODO: Currently making a separate buffer for each mesh, should have a batched version/auto-batch things
};

#endif
