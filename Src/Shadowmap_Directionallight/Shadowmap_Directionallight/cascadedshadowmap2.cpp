
#include "../../../../../Common/Common/common.h"
using namespace common;
#include "../../../../../Common/Graphic11/graphic11.h"
#include "../../../../../Common/Framework11/framework11.h"
#include "cascadedshadowmap2.h"


using namespace graphic;

cCascadedShadowMap2::cCascadedShadowMap2()
	: m_shadowMapSize(1024)
	, m_lightCams{ cCamera3D("shadow camera1")
			, cCamera3D("shadow camera2")
			, cCamera3D("shadow camera3") }
	, m_antiFlickerOn(true)
{
}

cCascadedShadowMap2::~cCascadedShadowMap2()
{
}


bool cCascadedShadowMap2::Create(cRenderer &renderer
	, const float shadowMapSize //= 1024
	, const float z0 //= 30.f
	, const float z1 //= 100.f
	, const float z2 //= 300.f
)
{
	m_shadowMapSize = shadowMapSize;
	m_splitZ[0] = z0;
	m_splitZ[1] = z1;
	m_splitZ[2] = z2;

	cViewport svp = renderer.m_viewPort;
	svp.m_vp.MinDepth = 0.f;
	svp.m_vp.MaxDepth = 1.f;
	svp.m_vp.Width = shadowMapSize;
	svp.m_vp.Height = shadowMapSize;

	m_shadowMaps.Create(renderer, eDepthBufferType::ARRAY, svp, 3, false);

	for (int i = 0; i < SHADOWMAP_COUNT; ++i)
	{
		m_frustums[i].SetFrustum(GetMainCamera().GetViewProjectionMatrix());
		m_lightCams[i].SetProjectionOrthogonal(shadowMapSize, shadowMapSize, 1, 1000000);
		m_arrBoundRadius[i] = 0.0f;
		m_arrBoundCenter[i] = Vector3(0, 0, 0);
	}

	return true;
}


bool cCascadedShadowMap2::UpdateParameter(cRenderer &renderer, const cCamera &camera)
{
	Vector3 vWorldCenter = camera.GetEyePos() + camera.GetDirection() * m_splitZ[2] * 0.5f;
	Matrix44 shadowView;
	shadowView.SetView(vWorldCenter, GetMainLight().GetDirection(), Vector3(0, 1, 0));

	cBoundingSphere bsphere = camera.GetBoundingSphere(camera.m_near, m_splitZ[2]);
	const float shadowBoundRadius = max(m_shadowBoundingSphere.GetRadius(), bsphere.GetRadius()) / 2.f;
	m_shadowBoundingSphere.SetRadius(shadowBoundRadius);
	m_shadowBoundingSphere.SetPos(bsphere.GetPos());

	Matrix44 shadowProj;
	shadowProj.SetProjectionOrthogonal(shadowBoundRadius, shadowBoundRadius, -shadowBoundRadius, shadowBoundRadius);

	const Matrix44 worldToShadowSpace = shadowView * shadowProj;

	cFrustum frustums[SHADOWMAP_COUNT];
	cFrustum::Split3_2(camera, m_splitZ[0], m_splitZ[1], m_splitZ[2]
		, frustums[0], frustums[1], frustums[2]);

	const Matrix44 shadowViewInv = shadowView.Inverse();

	float arrRanges[4];
	arrRanges[0] = camera.m_near;
	arrRanges[1] = m_splitZ[0];
	arrRanges[2] = m_splitZ[1];
	arrRanges[3] = m_splitZ[2];

	for (int cascadeIdx = 0; cascadeIdx < SHADOWMAP_COUNT; ++cascadeIdx)
	{
		Matrix44 cascadeTrans;
		Matrix44 cascadeScale;

		if (m_antiFlickerOn)
		{
			// Expend the radius to compensate for numerical errors
			cBoundingSphere tmpBSphere = camera.GetBoundingSphere(arrRanges[cascadeIdx], arrRanges[cascadeIdx + 1]);
			m_arrBoundRadius[cascadeIdx] = max(m_arrBoundRadius[cascadeIdx], tmpBSphere.GetRadius()) / 2.f;

			// Only update the cascade bounds if it moved at least a full pixel unit
			// This makes the transformation invariant to translation
			Vector3 vOffset;
			if (CascadeNeedsUpdate(shadowView, cascadeIdx, tmpBSphere.GetPos(), vOffset))
			{
				// To avoid flickering we need to move the bound center in full units
				const Vector3 vOffsetOut = vOffset.MultiplyNormal2(shadowViewInv);
				m_arrBoundCenter[cascadeIdx] += vOffsetOut;
			}

			// Get the cascade center in shadow space
			const Vector3 vCascadeCenterShadowSpace = m_arrBoundCenter[cascadeIdx] * worldToShadowSpace;

			// Update the translation from shadow to cascade space
			m_offsetX[cascadeIdx] = -vCascadeCenterShadowSpace.x;
			m_offsetY[cascadeIdx] = -vCascadeCenterShadowSpace.y;
			cascadeTrans.SetPosition(Vector3(m_offsetX[cascadeIdx], m_offsetY[cascadeIdx], 0.0f));

			// Update the scale from shadow to cascade space
			m_scale[cascadeIdx] = shadowBoundRadius / m_arrBoundRadius[cascadeIdx];
			cascadeScale.SetScale(Vector3(m_scale[cascadeIdx], m_scale[cascadeIdx], 1.0f));
		}
		else
		{

			Vector3 arrFrustumPoints[8];
			frustums[cascadeIdx].GetVertices(arrFrustumPoints);

			// Transform to shadow space and extract the minimum and maximum
			Vector3 vMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
			Vector3 vMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			for (int k = 0; k < 8; k++)
			{
				const Vector3 vPointInShadowSpace = arrFrustumPoints[k] * worldToShadowSpace;

				for (int m = 0; m < 3; m++)
				{
					if ((&vMin.x)[m] >(&vPointInShadowSpace.x)[m])
						(&vMin.x)[m] = (&vPointInShadowSpace.x)[m];
					if ((&vMax.x)[m] < (&vPointInShadowSpace.x)[m])
						(&vMax.x)[m] = (&vPointInShadowSpace.x)[m];
				}
			}

			Vector3 vCascadeCenterShadowSpace = (vMin + vMax) * 0.5f;

			// Update the translation from shadow to cascade space
			m_offsetX[cascadeIdx] = -vCascadeCenterShadowSpace.x;
			m_offsetY[cascadeIdx] = -vCascadeCenterShadowSpace.y;
			cascadeTrans.SetPosition(Vector3(m_offsetX[cascadeIdx], m_offsetY[cascadeIdx], 0.0f));

			// Update the scale from shadow to cascade space
			m_scale[cascadeIdx] = 2.0f / max(vMax.x - vMin.x, vMax.y - vMin.y);
			cascadeScale.SetScale(Vector3(m_scale[cascadeIdx], m_scale[cascadeIdx], 1.0f));
		}

		// Combine the matrices to get the transformation from world to cascade space
		m_worldToShadowProj[cascadeIdx] = worldToShadowSpace * cascadeTrans * cascadeScale;
	}

	// Set the values for the unused slots to someplace outside the shadow space
	for (int i = SHADOWMAP_COUNT; i < 4; i++)
	{
		m_offsetX[i] = 250.0f;
		m_offsetY[i] = 250.0f;
		m_scale[i] = 0.1f;
	}

	m_worldToShadowSpace = worldToShadowSpace;

	return true;
}


// Test if a cascade needs an update
bool cCascadedShadowMap2::CascadeNeedsUpdate(const Matrix44& mShadowView, int cascadeIdx
	, const Vector3& newCenter, OUT Vector3& vOffset)
{
	// Find the offset between the new and old bound ceter
	const Vector3 vOldCenterInCascade = m_arrBoundCenter[cascadeIdx] * mShadowView;
	const Vector3 vNewCenterInCascade = newCenter * mShadowView;
	const Vector3 vCenterDiff = vNewCenterInCascade - vOldCenterInCascade;

	// Find the pixel size based on the diameters and map pixel size
	const float fPixelSize = (float)m_shadowMapSize / (2.0f * m_arrBoundRadius[cascadeIdx]);
	const float fPixelOffX = vCenterDiff.x * fPixelSize;
	const float fPixelOffY = vCenterDiff.y * fPixelSize;

	// Check if the center moved at least half a pixel unit
	const bool bNeedUpdate = fabs(fPixelOffX) > 0.5f || fabs(fPixelOffY) > 0.5f;
	if (bNeedUpdate)
	{
		// Round to the 
		vOffset.x = floorf(0.5f + fPixelOffX) / fPixelSize;
		vOffset.y = floorf(0.5f + fPixelOffY) / fPixelSize;
		vOffset.z = vCenterDiff.z;
	}

	return bNeedUpdate;
}


// Prepare Render ShadowMap
bool cCascadedShadowMap2::Begin(cRenderer &renderer
	, const bool isClear //= true
)
{
	renderer.UnbindTextureAll();
	m_shadowMaps.Begin(renderer, isClear);
	return true;
}


// End Render ShadowMap
bool cCascadedShadowMap2::End(cRenderer &renderer)
{
	m_shadowMaps.End(renderer);
	return true;
}


bool cCascadedShadowMap2::Bind(cRenderer &renderer)
{
	m_shadowMaps.Bind(renderer, 4);
	return true;
}


// Debug Render
void cCascadedShadowMap2::Render(cRenderer &renderer)
{
	m_shadowMaps.Render(renderer);
}


void cCascadedShadowMap2::BuildShadowMap(cRenderer &renderer, cNode *node
	, const XMMATRIX &parentTm // = XMIdentity
	, const bool isClear //= true;
)
{
	//node->SetTechnique("BuildShadowMap");

	//UpdateParameter(renderer, GetMainCamera());
	//for (int i = 0; i < cCascadedShadowMap2::SHADOWMAP_COUNT; ++i)
	//{
	//	node->CullingTest(m_frustums[i], parentTm, true);

	//	Begin(renderer, i, isClear);
	//	node->Render(renderer, parentTm, eRenderFlag::SHADOW);
	//	End(renderer, i);
	//}

	//cFrustum frustum;
	//frustum.SetFrustum(GetMainCamera().GetViewProjectionMatrix());
	//node->CullingTest(frustum, parentTm, true); // Recovery Culling
}


void cCascadedShadowMap2::RenderShadowMap(cRenderer &renderer, cNode *node
	, const XMMATRIX &parentTm // = XMIdentity
)
{
	//node->SetTechnique("ShadowMap");
	//Bind(renderer);
	//node->Render(renderer, parentTm, eRenderFlag::TERRAIN);
	//node->Render(renderer, parentTm, eRenderFlag::MODEL | eRenderFlag::NOALPHABLEND);
}
