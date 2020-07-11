#include "meshnode.h"

#include <vector>
#include <stdexcept>
#include <algorithm>

MeshNode::MeshNode()
{
}

MeshNode::MeshNode(const std::vector<Renderer::VertexData>& meshData) {
	mMesh.reset(new Renderer::Mesh(meshData));
}

MeshNode::~MeshNode()
{
}

void MeshNode::doInit()
{
}

void MeshNode::doUpload()
{
}

void MeshNode::doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat)
{
	mat4x4 mvp(1.0f);
	mvp *= nodeMat;
	mvp *= viewMat;
	mvp *= projMat;

	rend.renderMesh(mMesh, mvp);
}
