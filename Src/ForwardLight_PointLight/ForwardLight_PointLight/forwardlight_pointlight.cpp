//
// DX11 ForwardLight - Point Light
//
// HLSL-Development-Cookbook
//	- Point light
// 

#include "../../../../../Common/Common/common.h"
using namespace common;
#include "../../../../../Common/Graphic11/graphic11.h"
#include "../../../../../Common/Framework11/framework11.h"

using namespace graphic;

struct sCBDirLightPS
{
	XMVECTOR AmbientDown;
	XMVECTOR AmbientRange;
};

static const char *g_hlslPath = "../Media/forwardlight_pointlight/hlsl.fxo";

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


public:
	cCamera3D m_camera;
	cGridLine m_ground;
	cModel m_model;
	cConstantBuffer<sCBDirLightPS> m_cbDirLight;

	int m_renderType; //0=new, 1=old
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
{
	m_windowName = L"DX11 ForwardLight - Point Light";
	const RECT r = { 0, 0, 1280, 1024 };
	m_windowRect = r;
	m_moveLen = 0;
	m_mouseDown[0] = false;
	m_mouseDown[1] = false;
	m_mouseDown[2] = false;
}

cViewer::~cViewer()
{
	graphic::ReleaseRenderer();
}


bool cViewer::OnInit()
{
	const float WINSIZE_X = m_windowRect.right - m_windowRect.left;
	const float WINSIZE_Y = m_windowRect.bottom - m_windowRect.top;
	GetMainCamera().SetCamera(Vector3(30, 30, -30), Vector3(0, 0, 0), Vector3(0, 1, 0));
	GetMainCamera().SetProjection(MATH_PI / 4.f, WINSIZE_X / WINSIZE_Y, 0.1f, 10000.0f);
	GetMainCamera().SetViewPort(WINSIZE_X, WINSIZE_Y);

	m_camera.SetCamera(Vector3(-3, 10, -10), Vector3(0, 0, 0), Vector3(0, 1, 0));
	m_camera.SetProjection(MATH_PI / 4.f, WINSIZE_X / WINSIZE_Y, .1f, 10000.f);
	m_camera.SetViewPort(WINSIZE_X, WINSIZE_Y);

	m_ground.Create(m_renderer, 10, 10, 1, 1);

	GetMainLight().Init(cLight::LIGHT_DIRECTIONAL,
		Vector4(0.2f, 0.2f, 0.2f, 1), Vector4(0.9f, 0.9f, 0.9f, 1),
		Vector4(0.2f, 0.2f, 0.2f, 1));
	const Vector3 lightPos(-3, 0, -3);
	//const Vector3 lightLookat(0, 0, 0);
	GetMainLight().SetPosition(lightPos);
	//GetMainLight().SetDirection((lightLookat - lightPos).Normal());

	m_model.Create(m_renderer, 0, "chessqueen.x");
	m_model.m_transform.pos.y = 0.1f;
	m_model.m_transform.scale *= 10.f;

	m_cbDirLight.Create(m_renderer);

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

	if (m_model.IsLoadFinish())
	{
		for (auto &mesh : m_model.m_model->m_meshes)
		{
			mesh->m_shader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_hlslPath
				, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0, false);
			mesh->m_isBeginShader = false;
		}
	}

	// Render
	if (m_renderer.ClearScene())
	{
		m_renderer.BeginScene();

		GetMainCamera().Bind(m_renderer);
		GetMainLight().Bind(m_renderer);

		m_ground.Render(m_renderer);

		cShader11 *shader = m_renderer.m_shaderMgr.LoadShader(m_renderer, g_hlslPath
			, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0, false);

		shader->SetTechnique((m_renderType == 0) ? "Unlit" : "Unlit_Old");
		shader->Begin();
		shader->BeginPass(m_renderer, 0);

		const Vector3 vAmbientLowerColor = Vector3(0.1f, 0.5f, 0.1f);
		const Vector3 vAmbientUpperColor = Vector3(0.1f, 0.2f, 0.5f);
		m_cbDirLight.m_v->AmbientDown = XMLoadFloat3((XMFLOAT3*)&GammaToLinear(vAmbientLowerColor));
		m_cbDirLight.m_v->AmbientRange = XMLoadFloat3((XMFLOAT3*)&(GammaToLinear(vAmbientUpperColor) - GammaToLinear(vAmbientLowerColor)));
		m_cbDirLight.Update(m_renderer, 6);

		m_model.Render(m_renderer);

		m_renderer.RenderFPS();
		m_renderer.EndScene();
		m_renderer.Present();
	}
}


void cViewer::OnLostDevice()
{
	m_renderer.ResetDevice(0, 0, true);
	m_camera.SetViewPort(m_renderer.m_viewPort.GetWidth(), m_renderer.m_viewPort.GetHeight());
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

