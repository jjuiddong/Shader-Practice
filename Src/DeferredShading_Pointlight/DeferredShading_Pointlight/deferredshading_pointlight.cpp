//
// DX11 Deferred Shading - Point Light
//
// HLSL-Development-Cookbook
//	- Point light
// 

#include "../../../../../Common/Common/common.h"
using namespace common;
#include "../../../../../Common/Graphic11/graphic11.h"
#include "../../../../../Common/Framework11/framework11.h"
#include "gbuffer.h"

using namespace graphic;


struct sCbDirightPS
{
	XMVECTOR AmbientDown;
	XMVECTOR AmbientRange;
};

struct sCbPointLight
{
	XMVECTOR PointLightPos;
	XMVECTOR PointLightRangeRcp;
	XMVECTOR PointColor;
	XMVECTOR LightPerspectiveValues;
	XMMATRIX LightProjection;
};


static const char *g_hlslPath = "../Media/deferredshading_pointlight/hlsl.fxo";
static const char *g_dirlightPath = "../Media/deferredshading_pointlight/dirlight.fxo";
static const char *g_deferredShaderPath = "../Media/deferredshading_pointlight/deferredshading.fxo";

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
	void RenderDirectionalLight();
	void RenderPointLight(const int lightIdx);


public:
	cCamera3D m_camera;
	cGridLine m_ground;
	cModel m_model[64];
	cImGui m_gui;
	cGBuffer m_gbuff;

	cConstantBuffer<sCbDirightPS> m_cbDirLight;
	cConstantBuffer<sCbPointLight> m_cbPointLight;	
	Vector3 m_ambientDown;
	Vector3 m_ambientUp;
	ID3D11RasterizerState* m_pNoDepthClipFrontRS;
	ID3D11DepthStencilState* m_pNoDepthWriteLessStencilMaskState;
	ID3D11DepthStencilState* m_pNoDepthWriteGreatherStencilMaskState;
	ID3D11BlendState* m_pAdditiveBlendState;

	int m_renderType; //0=new, 1=old
	bool m_isAnimate;
	Vector3 m_dirLightPos;
	Vector3 m_pointLightPos[4];
	float m_pointLightRange;
	Vector3 m_pointLightColor[4];

	Vector3 m_capsuleLightLength;
	Vector3 m_capuselLightRange;

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
	m_windowName = L"DX11 DeferredShading - Point Light";
	const RECT r = { 0, 0, 1280, 960 };
	m_windowRect = r;
	m_moveLen = 0;
	m_mouseDown[0] = false;
	m_mouseDown[1] = false;
	m_mouseDown[2] = false;

	m_capsuleLightLength.x = 1.f;
	m_capuselLightRange.x = 4.f;

	m_ambientDown = Vector3(0.f, 0.f, 0.f);
	m_ambientUp = Vector3(0.f, 0.f, 0.f);
}

cViewer::~cViewer()
{
	SAFE_RELEASE(m_pNoDepthWriteLessStencilMaskState);
	SAFE_RELEASE(m_pNoDepthWriteGreatherStencilMaskState);
	SAFE_RELEASE(m_pNoDepthClipFrontRS);
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
	m_camera.SetProjection(MATH_PI / 4.f, WINSIZE_X / WINSIZE_Y, 0.1f, 10000.0f);
	m_camera.SetViewPort(WINSIZE_X, WINSIZE_Y);

	m_ground.Create(m_renderer, 10, 10, 1, 1);

	m_gbuff.Create(m_renderer, (UINT)WINSIZE_X, (UINT)WINSIZE_Y);

	m_gui.Init(m_hWnd, m_renderer.GetDevice(), m_renderer.GetDevContext(), NULL);

	m_cbDirLight.Create(m_renderer);
	m_cbPointLight.Create(m_renderer);

	GetMainLight().Init(cLight::LIGHT_DIRECTIONAL,
		Vector4(0.f, 0.f, 0.f, 1), Vector4(0.f, 0.f, 0.f, 1),
		Vector4(0.f, 0.f, 0.f, 1));
	const Vector3 lightPos(-1, 300.f, -1);
	const Vector3 lightLookat(1, 300.f, 1);
	GetMainLight().SetPosition(lightPos);
	GetMainLight().SetDirection((lightLookat - lightPos).Normal());
	m_dirLightPos = lightPos;
	m_pointLightPos[0] = Vector3(0, 1.5f, 1.5f);
	m_pointLightPos[1] = Vector3(1.5f, 1.5f, 0.f);
	m_pointLightPos[2] = Vector3(-1.5f, 1.5f, 0);
	m_pointLightPos[3] = Vector3(0, 1.5f, -1.5f);
	m_pointLightRange = 3.f;
	m_pointLightColor[0] = Vector3(1, 1, 1);
	m_pointLightColor[1] = Vector3(1, 0, 0);
	m_pointLightColor[2] = Vector3(0, 1, 0);
	m_pointLightColor[3] = Vector3(0, 0, 1);

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

	D3D11_RASTERIZER_DESC descRast = {
		D3D11_FILL_SOLID,
		D3D11_CULL_FRONT,
		FALSE,
		D3D11_DEFAULT_DEPTH_BIAS,
		D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		FALSE,
		FALSE,
		FALSE,
		FALSE
	};
	descRast.CullMode = D3D11_CULL_FRONT;
	if (FAILED(m_renderer.GetDevice()->CreateRasterizerState(&descRast, &m_pNoDepthClipFrontRS)))
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

	m_gui.NewFrame();

	if (ImGui::Begin("Information", NULL, ImVec2(300, 600)))
	{
		ImGui::Checkbox("Animate", &m_isAnimate);
		ImGui::DragFloat("Range", &m_pointLightRange, 0.01f, 0.f, 100.f);
		ImGui::ColorEdit3("Point Light Color1", (float*)&m_pointLightColor[0]);
		ImGui::ColorEdit3("Point Light Color2", (float*)&m_pointLightColor[1]);
		ImGui::ColorEdit3("Point Light Color3", (float*)&m_pointLightColor[2]);
		ImGui::ColorEdit3("Point Light Color4", (float*)&m_pointLightColor[3]);

		ImGui::ColorEdit3("Ambient Down", (float*)&m_ambientDown);
		ImGui::ColorEdit3("Ambient Up", (float*)&m_ambientUp);
		ImGui::DragFloat("Specular Intensity Exp", &GetMainLight().m_specExp, 0.001f, 0.f, 200.f);
		ImGui::DragFloat("Specular Intensity", &GetMainLight().m_specIntensity, 0.001f, 0.f, 1.f);
		ImGui::End();
	}

	for (int i = 0; i < 64; ++i)
	{
		if (m_model[i].IsLoadFinish())
		{
			for (auto &mesh : m_model[i].m_model->m_meshes)
			{
				mesh->m_shader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_deferredShaderPath
					, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0, false);
				mesh->m_isBeginShader = false;
			}
		}
	}

	// Render Deferred Shading to GBuffer
	if (m_gbuff.Begin(m_renderer))
	{
		GetMainCamera().Bind(m_renderer);

		const float angle = deltaSeconds * 1.f * (m_isAnimate ? 1.0f : 0.f);
		Matrix44 tm;
		tm.SetRotationY(angle);

		for (int i = 0; i < 4; ++i)
		{
			const Vector3 lightPos = m_pointLightPos[i] * tm;
			const Vector3 lightLookat(0, lightPos.y, 0);
			const Vector3 norm = (lightLookat - lightPos).Normal();
			const Vector3 lightDir = (Vector3(0, 1, 0).CrossProduct(-norm)).Normal();
			const Vector3 lightStartPos = lightPos - (lightDir * (m_capsuleLightLength.x / 2.f));

			m_pointLightPos[i] = lightPos;
			GetMainLight().SetPosition(lightStartPos);
			GetMainLight().SetDirection(lightDir);
			GetMainLight().Bind(m_renderer);

			m_renderer.m_dbgSphere.SetPos(lightPos);
			m_renderer.m_dbgSphere.SetRadius(0.1f);
			m_renderer.m_dbgSphere.Render(m_renderer);
		}

		m_ground.Render(m_renderer);

		cShader11 *deferredShader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_deferredShaderPath
			, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0, false);

		deferredShader->SetTechnique((m_renderType == 0) ? "Unlit" : "Unlit_Old");
		deferredShader->Begin();
		deferredShader->BeginPass(m_renderer, 0);

		for (int i = 0; i < 64; ++i)
			m_model[i].Render(m_renderer);

		m_gbuff.End(m_renderer);
	}

	// Render to Main TargetBuffer
	{
		GetMainCamera().Bind(m_renderer);
		GetMainLight().Bind(m_renderer);

		ID3D11DeviceContext *devContext = m_renderer.GetDevContext();

		ID3D11DepthStencilState* pPrevDepthState;
		UINT nPrevStencil;
		devContext->OMGetDepthStencilState(&pPrevDepthState, &nPrevStencil);

		RenderDirectionalLight();

		//CommonStates state(m_renderer.GetDevice());
		//float factor[4] = { 1,1,1,1 };
		//devContext->OMSetBlendState(state.Additive(), factor, 0xffffffff);
		ID3D11BlendState* pPrevBlendState;
		FLOAT prevBlendFactor[4];
		UINT prevSampleMask;
		devContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
		devContext->OMSetBlendState(m_pAdditiveBlendState, prevBlendFactor, prevSampleMask);

		for (int i=0; i < 4; ++i)
			RenderPointLight(i);
		//devContext->OMSetBlendState(state.NonPremultiplied(), factor, 0xffffffff);

		devContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
		SAFE_RELEASE(pPrevBlendState);

		devContext->OMSetDepthStencilState(pPrevDepthState, nPrevStencil);
		SAFE_RELEASE(pPrevDepthState);
	}

	// Render GBuffer
	{
		ID3D11DeviceContext *devContext = m_renderer.GetDevContext();
		devContext->OMSetRenderTargets(1, &m_renderer.m_renderTargetView, NULL);
		m_gbuff.Render(m_renderer);
	}

	m_gui.Render();
	m_renderer.RenderFPS();
	m_renderer.Present();
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
	m_cbDirLight.Update(m_renderer, 6);
	m_gbuff.m_cbGBuffer.Update(m_renderer, 7);

	devContext->IASetInputLayout(NULL);
	devContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	devContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	devContext->Draw(4, 0);

	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	devContext->PSSetShaderResources(4, 1, arrRV);
	ZeroMemory(arrViews, sizeof(arrViews));
	devContext->PSSetShaderResources(0, 4, arrViews);
}


void cViewer::RenderPointLight(const int lightIdx)
{
	const float lightRange = m_pointLightRange;
	const Vector3 lightPos = m_pointLightPos[lightIdx];
	const Vector3 lightScale(lightRange, lightRange, lightRange);
	const Vector3 lightColor = m_pointLightColor[lightIdx];

	ID3D11DeviceContext *devContext = m_renderer.GetDevContext();
	cShader11 *hlslShader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_hlslPath, 0, false);
	hlslShader->SetTechnique("Unlit");
	hlslShader->Begin();
	hlslShader->BeginPass(m_renderer, 0);

	ID3D11RasterizerState* pPrevRSState;
	devContext->RSGetState(&pPrevRSState);
	devContext->RSSetState(m_pNoDepthClipFrontRS);
	devContext->OMSetDepthStencilState(m_pNoDepthWriteGreatherStencilMaskState, 1);

	ID3D11ShaderResourceView* arrViews[4] = { m_gbuff.m_DepthStencilSRV
		, m_gbuff.m_ColorSpecIntensitySRV
		, m_gbuff.m_NormalSRV
		, m_gbuff.m_SpecPowerSRV };
	devContext->PSSetShaderResources(0, 4, arrViews);

	m_renderer.m_cbPerFrame.Update(m_renderer);
	m_renderer.m_cbLight.Update(m_renderer, 1);

	m_cbDirLight.m_v->AmbientDown = XMLoadFloat3((XMFLOAT3*)&GammaToLinear(m_ambientDown));
	m_cbDirLight.m_v->AmbientRange = XMLoadFloat3((XMFLOAT3*)&(GammaToLinear(m_ambientUp) - GammaToLinear(m_ambientDown)));
	m_cbDirLight.Update(m_renderer, 6);
	m_gbuff.m_cbGBuffer.Update(m_renderer, 7);

	m_cbPointLight.m_v->PointLightPos = lightPos.GetVectorXM();
	m_cbPointLight.m_v->PointLightRangeRcp = (Vector3(1, 1, 1) / lightRange).GetVectorXM();
	m_cbPointLight.m_v->PointColor = GammaToLinear(lightColor).GetVectorXM();
	m_cbPointLight.m_v->LightPerspectiveValues = Vector3(0, 0, 0).GetVectorXM();

	Matrix44 lightProj;
	Matrix44 S;
	S.SetScale(lightScale);
	Matrix44 T;
	T.SetTranslate(lightPos);
	lightProj = S * T * GetMainCamera().GetViewProjectionMatrix();
	m_cbPointLight.m_v->LightProjection = XMMatrixTranspose(lightProj.GetMatrixXM());
	m_cbPointLight.Update(m_renderer, 8);

	devContext->IASetInputLayout(NULL);
	devContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	devContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);

	devContext->Draw(2, 0);

	devContext->RSSetState(pPrevRSState);
	SAFE_RELEASE(pPrevRSState);

	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	devContext->PSSetShaderResources(4, 1, arrRV);
	ZeroMemory(arrViews, sizeof(arrViews));
	devContext->PSSetShaderResources(0, 4, arrViews);
	m_renderer.UnbindShaderAll();
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

