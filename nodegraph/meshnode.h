#ifndef MESHNODE_H
#define MESHNODE_H

#include "node.h"

#include "renderer.h"

/**
 * A node which renders a mesh
 */
class MeshNode : public Node
{
public:
    MeshNode();
    MeshNode( const std::vector<Renderer::VertexData>& vertData );
	virtual ~MeshNode();

	// Render this node and any children
	void doInit(Renderer& rend) override;
	void doUpload(Renderer& rend) override;
	void doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat) override;
	void doCleanup(Renderer& rend) override;
private:
    std::vector<Renderer::VertexData> mVertData; // Cleared when mesh is created/uploaded
	std::shared_ptr<Renderer::Mesh> mMesh;
};

#endif
