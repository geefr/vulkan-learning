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
  if( mVertices.empty() || mIndices.empty() ) {
    // Don't create the buffers, renderer will reject
    // the mesh if it's asked to use it
    return;
  }
  mVertexBuffer = rend.createSimpleVertexBuffer(mVertices);
  std::memcpy(mVertexBuffer->map(), mVertices.data(), mVertices.size() * sizeof(decltype(mVertices)::value_type));
  mVertexBuffer->flush();
  mVertexBuffer->unmap();
  mVertices.clear();

  if( !mIndices.empty() ) {
    mIndexBuffer = rend.createSimpleIndexBuffer(mIndices);
    std::memcpy(mIndexBuffer->map(), mIndices.data(), mIndices.size() * sizeof(decltype(mIndices)::value_type));
    mIndexBuffer->flush();
    mIndexBuffer->unmap();
    mIndices.clear();
  }
}

void Mesh::cleanup(Renderer& rend) {
  mVertexBuffer.reset();
  mIndexBuffer.reset();
}

bool Mesh::validForRender() const { return mVertexBuffer && mIndexBuffer; }
