#include "meshnode.h"

#include <vector>
#include <stdexcept>
#include <algorithm>

MeshNode::MeshNode()
{
  // TODO: This is just a dummy so we see something
  mVertices = {
      {{-1.0, -1.0, 0.1},{0,0,-1}},
      {{ 1.0, -1.0, 0.1},{0,0,-1}},
      {{ 0.0,  1.0, 0.1},{0,0,-1}},
  };
  mIndices = { 0, 1, 2 };
}

MeshNode::~MeshNode()
{
}

MeshNode::MeshNode(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
  : mVertices(vertices)
  , mIndices(indices)
{
}

void MeshNode::doInit(Renderer& rend)
{
  mMesh.reset(new Mesh(mVertices, mIndices));
  mVertices.clear();
  mIndices.clear();
}

void MeshNode::doUpload(Renderer& rend)
{
  if (mMesh) mMesh->upload(rend);
}

void MeshNode::doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat)
{
  rend.renderMesh(mMesh, mMaterial, nodeMat);
}

void MeshNode::doCleanup(Renderer& rend) {
  if (mMesh) mMesh->cleanup(rend);
}

void MeshNode::mesh(std::shared_ptr<Mesh> mesh) { mMesh = mesh; }
void MeshNode::material(std::shared_ptr<Material> mat) { mMaterial = mat; }
