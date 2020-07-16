/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#include "mesh.h"

#include "renderer.h"

Mesh::Mesh(const std::vector<Vertex>& v, const std::vector<uint32_t>& i)
        : mVertices(v)
        , mIndices(i)
{}

void Mesh::upload(Renderer& rend) {
        mVertexBuffer = rend.createSimpleVertexBuffer(mVertices);
        std::memcpy(mVertexBuffer->map(), mVertices.data(), mVertices.size() * sizeof(decltype(mVertices)::value_type));
        mVertexBuffer->flush();
        mVertexBuffer->unmap();

        mIndexBuffer = rend.createSimpleIndexBuffer(mIndices);
        std::memcpy(mIndexBuffer->map(), mIndices.data(), mIndices.size() * sizeof(decltype(mIndices)::value_type));
        mIndexBuffer->flush();
        mIndexBuffer->unmap();
}

void Mesh::cleanup(Renderer& rend) {
  mVertexBuffer.reset();
}
