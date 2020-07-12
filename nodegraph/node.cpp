#include "node.h"
#include "renderer.h"

    Node::Node()
    {
    }

    Node::~Node()
    {
    }

    Node::Children& Node::children() { return mChildren; }
    vec3& Node::scale() { return mScale; }
    vec3& Node::rotation() { return mRot; }
    vec3& Node::translation() { return mTrans; }

    vec3& Node::scaleDelta() { return mScaleDelta; }
    vec3& Node::rotationDelta() { return mRotDelta; }
    vec3& Node::translationDelta() { return mTransDelta; }

    mat4x4 Node::matrix() const
    {
        mat4x4 m(1.0);

        m *= mUserModelMat;

        m = glm::translate(m, mTrans);

        m = rotate(m, mRot[0], vec3(1.f,0.f,0.f));
        m = rotate(m, mRot[1], vec3(0.f,1.f,0.f));
        m = rotate(m, mRot[2], vec3(0.f,0.f,1.f));

        m = glm::scale(m, mScale);
        
        return m;
    }

    void Node::userModelMatrix(mat4x4 mat) { mUserModelMat = mat; }

    vec3 Node::modelVecToWorldVec( vec3 v )
    {
        // Just dealing with a direction here
        // so don't care about scale/translate
        mat4x4 m(1.0);

        m *= mUserModelMat;
        m = rotate(m, mRot[0], - vec3(1.f,0.f,0.f));
        m = rotate(m, mRot[1], - vec3(0.f,1.f,0.f));
        m = rotate(m, mRot[2], - vec3(0.f,0.f,1.f));
        
        return vec4(v, 1.0f) * m;
    }

    void Node::init(Renderer& rend)
    {
        doInit(rend);
        for( auto& c: mChildren ) c->init(rend);
    }

    void Node::upload(Renderer& rend)
    {
        doUpload(rend);
        for( auto& c: mChildren ) c->upload(rend);
    }

    void Node::render(Renderer& rend, mat4x4 viewMat, mat4x4 projMat)
    {
      if( !mEnabled ) return;

      mat4x4 nodeMat(1.0f);
      render(rend, nodeMat, viewMat, projMat);
    }

    void Node::render(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat)
    {
      if( !mEnabled ) return;

      // Apply this node's transformation matrix
      nodeMat = nodeMat * matrix();

      doRender(rend, nodeMat, viewMat, projMat);
      for( auto& c: mChildren ) c->render(rend, nodeMat, viewMat, projMat);
    }

    void Node::update(Engine& eng, double deltaT)
    {
      if( !mEnabled ) return;
      
      if( mUpdateScript ) mUpdateScript(eng, *this, deltaT);

      doUpdate(deltaT);
      for( auto& c : mChildren ) c->update(eng, deltaT);
    }
    
    void Node::updateScript(UpdateScript s) { mUpdateScript = s; }

    void Node::cleanup(Renderer& rend) {
	doCleanup(rend);
	for( auto& c : mChildren) c->cleanup(rend);
    }

    void Node::doUpdate(double deltaT)
    {
      // Update if our transform is changing over time for some reason
      mTrans += mTransDelta * (float)deltaT;
      mRot += mRotDelta * (float)deltaT;
      mScale += mScaleDelta * (float)deltaT;
    }

    void Node::doInit(Renderer& rend) {}
    void Node::doUpload(Renderer& rend) {}
    void Node::doRender(Renderer& rend, mat4x4 nodeMat, mat4x4 viewMat, mat4x4 projMat) {}
    void Node::doCleanup(Renderer& rend) {}

    bool& Node::enabled() { return mEnabled; }

