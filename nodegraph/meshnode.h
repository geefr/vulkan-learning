#ifndef MESHNODE_H
#define MESHNODE_H

#include "node.h"

/**
 * A node which renders a mesh
 */
class MeshNode : public Node
{
public:
	MeshNode();
	MeshNode(const std::vector<Renderer::VertexData>& meshData);
	virtual ~MeshNode();

	// Render this node and any children
	void doInit() override;
	void doUpload() override;
	void doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat) override;
private:
	std::shared_ptr<Renderer::Mesh> mMesh;
};

#endif
