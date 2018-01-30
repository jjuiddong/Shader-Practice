//
// 2018-01-27, jjuiddong
// Cascaded ShadowMap for Terrain
// - Book Sample, HLSL Programming, ShadowMap Directional Lighting
//
#pragma once

#include "depthbufferarray.h"


namespace graphic
{

	class cCascadedShadowMap2
	{
	public:
		cCascadedShadowMap2();
		virtual ~cCascadedShadowMap2();

		bool Create(cRenderer &renderer
			, const float shadowMapSize = 1024
			, const float z0 = 30.f
			, const float z1 = 100.f
			, const float z2 = 300.f
		);
		bool UpdateParameter(cRenderer &renderer, const cCamera &camera);
		bool Bind(cRenderer &renderer);
		bool Begin(cRenderer &renderer, const bool isClear = true);
		bool End(cRenderer &renderer);
		void BuildShadowMap(cRenderer &renderer, cNode *node, const XMMATRIX &parentTm = XMIdentity, const bool isClear = true);
		void RenderShadowMap(cRenderer &renderer, cNode *node, const XMMATRIX &parentTm = XMIdentity);
		void Render(cRenderer &renderer);


	protected:
		bool CascadeNeedsUpdate(const Matrix44& mShadowView, int cascadeIdx
			, const Vector3& newCenter, OUT Vector3& vOffset);


	public:
		enum { SHADOWMAP_COUNT = 3 }; // maximum 4
		float m_shadowMapSize;
		float m_splitZ[3];
		cFrustum m_frustums[SHADOWMAP_COUNT];
		cCamera3D m_lightCams[SHADOWMAP_COUNT];
		cDepthBufferArray m_shadowMaps;
		cBoundingSphere m_shadowBoundingSphere;
		Matrix44 m_worldToShadowSpace;
		Matrix44 m_worldToShadowProj[SHADOWMAP_COUNT];
		float m_arrBoundRadius[SHADOWMAP_COUNT];
		Vector3 m_arrBoundCenter[SHADOWMAP_COUNT];
		float m_offsetX[4];
		float m_offsetY[4];
		float m_scale[4];
		bool m_antiFlickerOn;
	};

}
