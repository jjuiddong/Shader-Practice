//
// DX11 Shadowmap - Directional Light
//
// HLSL-Development-Cookbook
//	- Directional light
// 

#include "../../../../../Common/Common/common.h"
using namespace common;
#include "../../../../../Common/Graphic11/graphic11.h"
#include "../../../../../Common/Framework11/framework11.h"
#include "gbuffer.h"
#include "depthbufferarray.h"
#include "cascadedshadowmap2.h"

using namespace graphic;


struct sCbDirightPS
{
	XMVECTOR AmbientDown;
	XMVECTOR AmbientRange;
	XMMATRIX ToShadowSpace;
	XMVECTOR ToCascadeSpace[3];
};

struct sCbCascadedShadowmap
{
	XMMATRIX CascadeViewProj[3];
};


static const char *g_hlslPath = "../Media/shadowmap_directionallight/hlsl.fxo";
static const char *g_dirlightPath = "../Media/shadowmap_directionallight/dirlight.fxo";
static const char *g_deferredShaderPath = "../Media/shadowmap_directionallight/deferredshading.fxo";
static const char *g_shadowShaderPath = "../Media/shadowmap_directionallight/shadowgen.fxo";

class cViewer : public framework::cGameMain
{
public:
	cViewer();
	virtual ~cViewer();

	virtual bool OnInit() override;
	virtual void OnUpdate(const float deltaSeconds) override;
	virtual void OnRender(const float deltaSeconds) override;
	virtual void OnLostDevice() override;
	virtual void OnShutdown() override;
	virtual void OnMessageProc(UINT message, WPARAM wParam, LPARAM lParam) override;
	void ChangeWindowSize();
	void UpdateLookAt();


protected:
	void GenerateShadowmap();
	void RenderDirectionalLight();


public:
	cCamera3D m_camera;
	cGridLine m_ground;
	cModel m_model[64];
	cQuad m_quad;
	cImGui m_gui;
	cGBuffer m_gbuff;
	cCascadedShadowMap2 m_ccsm;

	cConstantBuffer<sCbDirightPS> m_cbDirLight;
	cConstantBuffer<sCbCascadedShadowmap> m_cbCascadedShadowmap;	
	Vector3 m_ambientDown;
	Vector3 m_ambientUp;
	ID3D11RasterizerState* m_pNoDepthClipFrontRS;
	ID3D11RasterizerState* m_pShadowGenRS;
	ID3D11DepthStencilState* m_pShadowGenDepthState;
	ID3D11DepthStencilState* m_pNoDepthWriteLessStencilMaskState;
	ID3D11DepthStencilState* m_pNoDepthWriteGreatherStencilMaskState;
	ID3D11BlendState* m_pAdditiveBlendState;

	int m_renderType; //0=new, 1=old
	bool m_isAnimate;

	sf::Vector2i m_mousePos;
	float m_moveLen;
	Vector3 m_target;
	bool m_mouseDown[3]; //left-right-middle
};

INIT_FRAMEWORK(cViewer);


cViewer::cViewer()
	: m_camera("main camera")
	, m_renderType(0)
	, m_target(0, 0, 0)
	, m_isAnimate(false)
	, m_pNoDepthWriteLessStencilMaskState(NULL)
	, m_pNoDepthWriteGreatherStencilMaskState(NULL)
{
	m_windowName = L"DX11 Shadowmap - Directional Light";
	const RECT r = { 0, 0, 1280, 1024 };
	m_windowRect = r;
	m_moveLen = 0;
	m_mouseDown[0] = false;
	m_mouseDown[1] = false;
	m_mouseDown[2] = false;

	m_ambientDown = Vector3(0.f, 0.f, 0.f);
	m_ambientUp = Vector3(0.f, 0.f, 0.f);
}

cViewer::~cViewer()
{
	SAFE_RELEASE(m_pNoDepthWriteLessStencilMaskState);
	SAFE_RELEASE(m_pNoDepthWriteGreatherStencilMaskState);
	SAFE_RELEASE(m_pNoDepthClipFrontRS);
	SAFE_RELEASE(m_pShadowGenRS);
	SAFE_RELEASE(m_pAdditiveBlendState);
	graphic::ReleaseRenderer();
}


bool cViewer::OnInit()
{
	const float WINSIZE_X = m_windowRect.right - m_windowRect.left;
	const float WINSIZE_Y = m_windowRect.bottom - m_windowRect.top;
	GetMainCamera().SetCamera(Vector3(30, 30, -30), Vector3(0, 0, 0), Vector3(0, 1, 0));
	GetMainCamera().SetProjection(MATH_PI / 4.f, WINSIZE_X / WINSIZE_Y, 0.1f, 10000.0f);
	GetMainCamera().SetViewPort(WINSIZE_X, WINSIZE_Y);

	m_camera.SetCamera(Vector3(0, 10, -10), Vector3(0, 0, 0), Vector3(0, 1, 0));
	m_camera.SetProjection(MATH_PI / 4.f, WINSIZE_X / WINSIZE_Y, 0.1f, 1000.0f);
	m_camera.SetViewPort(WINSIZE_X, WINSIZE_Y);

	m_ground.Create(m_renderer, 10, 10, 1, 1);

	m_gui.Init(m_hWnd, m_renderer.GetDevice(), m_renderer.GetDevContext(), NULL);

	m_cbDirLight.Create(m_renderer);
	m_cbCascadedShadowmap.Create(m_renderer);

	GetMainLight().Init(cLight::LIGHT_DIRECTIONAL,
		Vector4(0.f, 0.f, 0.f, 1), Vector4(0.9f, 0.9f, 0.9f, 1),
		Vector4(0.2f, 0.2f, 0.2f, 1));
	const Vector3 lightPos(-10, 30.f, -10);
	const Vector3 lightLookat(0, 0.f, 0);
	GetMainLight().SetPosition(lightPos);
	GetMainLight().SetDirection((lightLookat - lightPos).Normal());

	int idx = 0;
	for (int x = 0; x < 8; ++x)
	{
		for (int z = 0; z < 8; ++z)
		{
			m_model[idx].Create(m_renderer, 0, "chessqueen.x");
			m_model[idx].SetRenderFlag(eRenderFlag::ALPHABLEND, false);
			m_model[idx].SetRenderFlag(eRenderFlag::NOALPHABLEND, true);
			m_model[idx].m_transform.pos = Vector3((x - 4)*1.f, 0, (z - 4)*1.f);
			m_model[idx].m_transform.pos.y = 0.1f;
			m_model[idx].m_transform.scale *= 10.f;
			++idx;
		}
	}

	m_quad.Create(m_renderer, 10, 10, Vector3(0, 0, 0)
		, (eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0)
		, g_defaultTexture);
	m_quad.m_transform.rot.SetRotationX(MATH_PI / 2.f);
	m_quad.m_transform.pos.y = 0.1f;

	m_gbuff.Create(m_renderer, (UINT)WINSIZE_X, (UINT)WINSIZE_Y);

	m_ccsm.Create(m_renderer, 1024, 5.f, 15.f, 50.f);

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP
		, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	if (FAILED(m_renderer.GetDevice()->CreateDepthStencilState(&descDepth, &m_pNoDepthWriteLessStencilMaskState)))
		return false;

	descDepth.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	if (FAILED(m_renderer.GetDevice()->CreateDepthStencilState(&descDepth, &m_pNoDepthWriteGreatherStencilMaskState)))
		return false;

	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.StencilEnable = FALSE;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	if (FAILED(m_renderer.GetDevice()->CreateDepthStencilState(&descDepth, &m_pShadowGenDepthState)))
		return false;

	D3D11_RASTERIZER_DESC descRast = {
		D3D11_FILL_SOLID,
		D3D11_CULL_FRONT,
		FALSE,
		D3D11_DEFAULT_DEPTH_BIAS,
		D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE,
		FALSE,
		FALSE,
		FALSE
	};
	descRast.CullMode = D3D11_CULL_FRONT;
	if (FAILED(m_renderer.GetDevice()->CreateRasterizerState(&descRast, &m_pNoDepthClipFrontRS)))
		return false;

	descRast.CullMode = D3D11_CULL_BACK;
	descRast.FillMode = D3D11_FILL_SOLID;
	descRast.DepthBias = 85;
	descRast.SlopeScaledDepthBias = 6.0f;
	descRast.DepthClipEnable = FALSE;
	if (FAILED(m_renderer.GetDevice()->CreateRasterizerState(&descRast, &m_pShadowGenRS)))
		return false;

	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[i] = defaultRenderTargetBlendDesc;
	if (FAILED(m_renderer.GetDevice()->CreateBlendState(&descBlend, &m_pAdditiveBlendState)))
		return false;

	return true;
}


void cViewer::OnUpdate(const float deltaSeconds)
{
	cAutoCam cam(&m_camera);
	GetMainCamera().Update(deltaSeconds);
}

const Vector3 GammaToLinear(const Vector3& color)
{
	return Vector3(color.x * color.x, color.y * color.y, color.z * color.z);
}


void cViewer::OnRender(const float deltaSeconds)
{
	cAutoCam cam(&m_camera);
	ID3D11DeviceContext *devContext = m_renderer.GetDevContext();

	m_gui.NewFrame();

	// UI
	if (ImGui::Begin("Information", NULL, ImVec2(300, 600)))
	{
		ImGui::Checkbox("Animate", &m_isAnimate);
		ImGui::ColorEdit3("Directional Light Color", (float*)&GetMainLight().m_diffuse);

		ImGui::ColorEdit3("Ambient Down", (float*)&m_ambientDown);
		ImGui::ColorEdit3("Ambient Up", (float*)&m_ambientUp);
		ImGui::DragFloat("Specular Intensity Exp", &GetMainLight().m_specExp, 0.001f, 0.f, 200.f);
		ImGui::DragFloat("Specular Intensity", &GetMainLight().m_specIntensity, 0.001f, 0.f, 1.f);
		ImGui::End();
	}

	// Animation
	{
		//const float angle = deltaSeconds * 1.f * (m_isAnimate ? 1.0f : 0.f);
		//Matrix44 tm;
		//tm.SetRotationY(angle);

		//for (int i = 0; i < 4; ++i)
		//{
		//	const Vector3 lightPos = m_DirectionalLightPos[i] * tm;
		//	const Vector3 lightLookat(0, 0, 0);
		//	const Vector3 lightDir = (lightLookat - lightPos).Normal();
		//	m_DirectionalLightPos[i] = lightPos;
		//	m_DirectionalLightDir[i] = lightDir;
		//	m_DirectionalLight[i].SetPosition(lightPos);
		//	m_DirectionalLight[i].SetDirection(lightDir);
		//}
	}

	GenerateShadowmap();

	// Update Model Option
	for (int i = 0; i < 64; ++i)
		if (m_model[i].IsLoadFinish())
			for (auto &mesh : m_model[i].m_model->m_meshes)
				mesh->m_isBeginShader = false;

	// Render Deferred Shading to GBuffer
	if (m_gbuff.Begin(m_renderer))
	{
		GetMainCamera().Bind(m_renderer);

		cShader11 *deferredShader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_deferredShaderPath
			, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0, false);

		deferredShader->SetTechnique((m_renderType == 0) ? "Unlit" : "Unlit_Old");
		deferredShader->Begin();
		deferredShader->BeginPass(m_renderer, 0);

		for (int i = 0; i < 64; ++i)
		{
			if (m_model[i].m_model)
			{
				m_model[i].SetShader(deferredShader);
				m_model[i].Render(m_renderer);
			}
		}

		m_quad.m_shader = deferredShader;
		m_quad.Render(m_renderer);
		//m_ground.Render(m_renderer);

		m_gbuff.End(m_renderer);
	}

	// Render to Main TargetBuffer
	m_renderer.UnbindShaderAll();
	m_renderer.ClearScene();
	m_renderer.BeginScene();
	if (1)
	{
		GetMainCamera().Bind(m_renderer);
		GetMainLight().Bind(m_renderer);

		ID3D11DepthStencilState* pPrevDepthState;
		UINT nPrevStencil;
		devContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);
		RenderDirectionalLight();

		ID3D11BlendState* pPrevBlendState;
		FLOAT prevBlendFactor[4];
		UINT prevSampleMask;
		devContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
		devContext->OMSetBlendState(m_pAdditiveBlendState, prevBlendFactor, prevSampleMask);

		devContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
		SAFE_RELEASE(pPrevBlendState);

		devContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
		SAFE_RELEASE(pPrevDepthState);
	}

	// Render GBuffer
	m_renderer.SetRenderTarget(NULL, NULL); // recovery
	//m_gbuff.Render(m_renderer);
	m_ccsm.Render(m_renderer);

	m_gui.Render();
	m_renderer.RenderFPS();
	m_renderer.EndScene();
	m_renderer.Present();
}


void cViewer::GenerateShadowmap()
{
	ID3D11DeviceContext *devContext = m_renderer.GetDevContext();

	// Generate Shadowmap
	ID3D11RasterizerState* pPrevRSState;
	devContext->RSGetState(&pPrevRSState);

	ID3D11DepthStencilState* pPrevDepthState;
	UINT nPrevStencil;
	devContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

	if (m_ccsm.Begin(m_renderer))
	{
		cShader11 *shadowShader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_shadowShaderPath
			, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0, false);

		shadowShader->SetTechnique("Unlit");
		shadowShader->Begin();
		shadowShader->BeginPass(m_renderer, 0);
		devContext->RSSetState(m_pShadowGenRS);
		devContext->OMSetDepthStencilState(m_pShadowGenDepthState, 0);

		m_ccsm.UpdateParameter(m_renderer, GetMainCamera());

		for (int i = 0; i < 3; ++i)
			m_cbCascadedShadowmap.m_v->CascadeViewProj[i] 
				= XMMatrixTranspose(m_ccsm.m_worldToShadowProj[i].GetMatrixXM());
		m_cbCascadedShadowmap.Update(m_renderer, 6);

		for (int i = 0; i < 64; ++i)
		{
			if (m_model[i].m_model)
			{
				m_model[i].SetShader(shadowShader);
				m_model[i].Render(m_renderer);
			}
		}

		m_quad.m_shader = shadowShader;
		m_quad.Render(m_renderer);
		m_ccsm.End(m_renderer);
	}

	devContext->RSSetState(pPrevRSState);
	SAFE_RELEASE(pPrevRSState);

	devContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
	SAFE_RELEASE(pPrevDepthState);
}


void cViewer::RenderDirectionalLight()
{
	ID3D11DeviceContext *devContext = m_renderer.GetDevContext();
	devContext->ClearRenderTargetView(m_renderer.m_renderTargetView
		, (float*)&Vector4(50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.0f));
	devContext->OMSetRenderTargets(1
		, &m_renderer.m_renderTargetView, m_gbuff.m_DepthStencilReadOnlyDSV);
	m_gbuff.PrepareForUnpack(m_renderer);

	devContext->OMSetDepthStencilState(m_pNoDepthWriteLessStencilMaskState, 1);

	cShader11 *dirLightShader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_dirlightPath, 0, false);
	dirLightShader->SetTechnique("Unlit");
	dirLightShader->Begin();
	dirLightShader->BeginPass(m_renderer, 0);

	ID3D11ShaderResourceView* arrViews[4] = { m_gbuff.m_DepthStencilSRV
		, m_gbuff.m_ColorSpecIntensitySRV
		, m_gbuff.m_NormalSRV
		, m_gbuff.m_SpecPowerSRV };
	devContext->PSSetShaderResources(0, 4, arrViews);

	m_renderer.m_cbPerFrame.Update(m_renderer);
	m_renderer.m_cbLight.Update(m_renderer, 1);

	m_cbDirLight.m_v->AmbientDown = XMLoadFloat3((XMFLOAT3*)&GammaToLinear(m_ambientDown));
	m_cbDirLight.m_v->AmbientRange = XMLoadFloat3((XMFLOAT3*)&(GammaToLinear(m_ambientUp) - GammaToLinear(m_ambientDown)));

	m_cbDirLight.m_v->ToCascadeSpace[0] =
		Vector4(m_ccsm.m_offsetX[0]
			, m_ccsm.m_offsetX[1]
			, m_ccsm.m_offsetX[2]
			, m_ccsm.m_offsetX[3]).GetVectorXM();

	m_cbDirLight.m_v->ToCascadeSpace[1] =
		Vector4(m_ccsm.m_offsetY[0]
			, m_ccsm.m_offsetY[1]
			, m_ccsm.m_offsetY[2]
			, m_ccsm.m_offsetY[3]).GetVectorXM();

	m_cbDirLight.m_v->ToCascadeSpace[2] =
		Vector4(m_ccsm.m_scale[0]
			, m_ccsm.m_scale[1]
			, m_ccsm.m_scale[2]
			, m_ccsm.m_scale[3]).GetVectorXM();

	m_cbDirLight.m_v->ToShadowSpace = XMMatrixTranspose(m_ccsm.m_worldToShadowSpace.GetMatrixXM());
	m_cbDirLight.Update(m_renderer, 6);
	
	m_gbuff.m_cbGBuffer.Update(m_renderer, 7);
	m_ccsm.Bind(m_renderer);

	devContext->IASetInputLayout(NULL);
	devContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	devContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	devContext->Draw(4, 0);

	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	devContext->PSSetShaderResources(4, 1, arrRV);
	ZeroMemory(arrViews, sizeof(arrViews));
	devContext->PSSetShaderResources(0, 4, arrViews);
}


void cViewer::OnLostDevice()
{
	m_renderer.ResetDevice(0, 0, true);
	m_camera.SetViewPort(m_renderer.m_viewPort.GetWidth(), m_renderer.m_viewPort.GetHeight());
	m_gui.CreateDeviceObjects();
}


void cViewer::OnShutdown()
{
}


void cViewer::ChangeWindowSize()
{
	if (m_renderer.CheckResetDevice())
	{
		m_renderer.ResetDevice();
		m_camera.SetViewPort(m_renderer.m_viewPort.GetWidth(), m_renderer.m_viewPort.GetHeight());
	}
}


void cViewer::UpdateLookAt()
{
	GetMainCamera().MoveCancel();

	const Plane ground(Vector3(0, 1, 0), 0);
	const float centerX = GetMainCamera().m_width / 2;
	const float centerY = GetMainCamera().m_height / 2;
	const Ray ray = GetMainCamera().GetRay((int)centerX, (int)centerY);
	const float distance = ground.Collision(ray.dir);
	if (distance < -0.2f)
	{
		GetMainCamera().m_lookAt = ground.Pick(ray.orig, ray.dir);
	}
	else
	{ // horizontal viewing
		const Vector3 lookAt = GetMainCamera().m_eyePos + GetMainCamera().GetDirection() * 50.f;
		GetMainCamera().m_lookAt = lookAt;
	}

	GetMainCamera().UpdateViewMatrix();
}


void cViewer::OnMessageProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	m_gui.WndProcHandler(m_hWnd, message, wParam, lParam);
	if (ImGui::IsAnyItemHovered())
	{
		if (WM_LBUTTONDOWN == message)
			SetCapture(m_hWnd);
		else if (WM_LBUTTONUP == message)
			ReleaseCapture();
		return;
	}

	static bool maximizeWnd = false;
	switch (message)
	{
	case WM_EXITSIZEMOVE:
		ChangeWindowSize();
		break;

	case WM_SIZE:
		if (SIZE_MAXIMIZED == wParam)
		{
			maximizeWnd = true;
			ChangeWindowSize();
		}
		else if (maximizeWnd && (SIZE_RESTORED == wParam))
		{
			maximizeWnd = false;
			ChangeWindowSize();
		}
		break;

	case WM_MOUSEWHEEL:
	{
		cAutoCam cam(&m_camera);
		const Plane ground(Vector3(0, 1, 0), 0);
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		const Ray ray = graphic::GetMainCamera().GetRay(m_mousePos.x, m_mousePos.y);
		const Vector3 lookPos = ground.Pick(ray.orig, ray.dir);
		const float len = GetMainCamera().GetEyePos().Distance(lookPos);
		const float zoomLen = len / 6.f;
		if ((zDelta > 0) && GetMainCamera().GetEyePos().y < 1.f)
			break;

		graphic::GetMainCamera().Zoom(ray.dir, (zDelta<0) ? -zoomLen : zoomLen);

	}
	break;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_TAB:
		{
			static bool flag = false;
			flag = !flag;
		}
		break;
		case '1': m_renderType = 0; break;
		case '2': m_renderType = 1; break;
		case VK_RETURN:
			break;
		}
		break;

	case WM_LBUTTONDOWN:
	{
		cAutoCam cam(&m_camera);

		const POINT pos = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

		SetCapture(m_hWnd);
		m_mouseDown[0] = true;
		m_mousePos = { (int)LOWORD(lParam), (int)HIWORD(lParam) };

		const Plane ground(Vector3(0, 1, 0), 0);
		const Ray ray = graphic::GetMainCamera().GetRay(pos.x, pos.y);
		const Vector3 p1 = ground.Pick(ray.orig, ray.dir);
		m_moveLen = common::clamp(1, 100, (p1 - ray.orig).Length());
		graphic::GetMainCamera().MoveCancel();
	}
	break;

	case WM_LBUTTONUP:
		ReleaseCapture();
		m_mouseDown[0] = false;
		break;

	case WM_RBUTTONDOWN:
	{
		cAutoCam cam(&m_camera);
		GetMainCamera().MoveCancel();

		const POINT pos = { (int)LOWORD(lParam), (int)HIWORD(lParam) };
		const Plane ground(Vector3(0, 1, 0), 0);
		const Ray ray = GetMainCamera().GetRay(pos.x, pos.y);
		m_target = ground.Pick(ray.orig, ray.dir);

		SetCapture(m_hWnd);
		m_mouseDown[1] = true;
		m_mousePos = { (int)LOWORD(lParam), (int)HIWORD(lParam) };
	}
	break;

	case WM_RBUTTONUP:
		m_mouseDown[1] = false;
		ReleaseCapture();
		break;

	case WM_MBUTTONDOWN:
		SetCapture(m_hWnd);
		m_mouseDown[2] = true;
		m_mousePos = { (int)LOWORD(lParam), (int)HIWORD(lParam) };
		break;

	case WM_MBUTTONUP:
		ReleaseCapture();
		m_mouseDown[2] = false;
		break;

	case WM_MOUSEMOVE:
	{
		cAutoCam cam(&m_camera);

		sf::Vector2i pos = { (int)LOWORD(lParam), (int)HIWORD(lParam) };
		const Plane ground(Vector3(0, 1, 0), 0);
		const Ray ray = graphic::GetMainCamera().GetRay(pos.x, pos.y);
		Vector3 p1 = ground.Pick(ray.orig, ray.dir);

		if (m_mouseDown[0])
		{
			const int x = pos.x - m_mousePos.x;
			const int y = pos.y - m_mousePos.y;

			if ((abs(x) > 1000) || (abs(y) > 1000))
			{
				break;
			}

			Vector3 dir = graphic::GetMainCamera().GetDirection();
			Vector3 right = graphic::GetMainCamera().GetRight();
			dir.y = 0;
			dir.Normalize();
			right.y = 0;
			right.Normalize();

			graphic::GetMainCamera().MoveRight(-x * m_moveLen * 0.001f);
			graphic::GetMainCamera().MoveFrontHorizontal(y * m_moveLen * 0.001f);
		}
		else if (m_mouseDown[1])
		{
			const int x = pos.x - m_mousePos.x;
			const int y = pos.y - m_mousePos.y;
			m_camera.Yaw3(x * 0.005f, m_target);
			m_camera.Pitch3(y * 0.005f, m_target);

		}
		else if (m_mouseDown[2])
		{
			const sf::Vector2i point = { pos.x - m_mousePos.x, pos.y - m_mousePos.y };
			const float len = graphic::GetMainCamera().GetDistance();
			graphic::GetMainCamera().MoveRight(-point.x * len * 0.001f);
			graphic::GetMainCamera().MoveUp(point.y * len * 0.001f);
		}

		m_mousePos = pos;
	}
	break;
	}
}

