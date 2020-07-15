#include "meshnode.h"

#include <vector>
#include <stdexcept>
#include <algorithm>

MeshNode::MeshNode()
{
    // TODO: This is just a dummy so we see something
    mVertData = {
        {{-1.0, -1.0, 0.0},{1,0,0,1}},
        {{ 1.0, -1.0, 0.0},{0,1,0,1}},
        {{ 0.0,  1.0, 0.0},{0,0,1,1}},
    };
}

MeshNode::~MeshNode()
{
}

MeshNode::MeshNode( const std::vector<Renderer::VertexData>& vertData )
  : mVertData(vertData)
{
}

void MeshNode::doInit(Renderer& rend)
{
    mMesh.reset(new Renderer::Mesh(mVertData));
    mVertData.clear();
}

void MeshNode::doUpload(Renderer& rend)
{
	if( mMesh ) mMesh->upload(rend);
}

void MeshNode::doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat)
{
	mat4x4 mvp(1.0f);
	mvp *= nodeMat;
	mvp *= viewMat;
	mvp *= projMat;

	rend.renderMesh(mMesh, mvp);
}

void MeshNode::doCleanup(Renderer& rend) {
  if( mMesh ) mMesh->cleanup(rend);
}
