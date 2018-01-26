
#include "../../../../../Common/Common/common.h"
using namespace common;
#include "../../../../../Common/Graphic11/graphic11.h"
#include "../../../../../Common/Framework11/framework11.h"
#include "cubedepthbuffer.h"

using namespace graphic;


cCubeDepthBuffer::cCubeDepthBuffer()
	: m_depthDSV(NULL)
	, m_texture(NULL)
	, m_depthSRV(NULL)
	, m_depthSRVArray(NULL)
{
}

cCubeDepthBuffer::~cCubeDepthBuffer()
{
	Clear();
}


bool cCubeDepthBuffer::Create(cRenderer &renderer
	, const cViewport viewPort
	, const bool isMultiSampling //= true
	, const DXGI_FORMAT texFormat //depth stecil view = DXGI_FORMAT_R32_TYPELESS
	, const DXGI_FORMAT SRVFormat //depth stecil view = DXGI_FORMAT_R32_FLOAT
	, const DXGI_FORMAT DSVFormat //depth stecil view = DXGI_FORMAT_D32_FLOAT
)
{
	Clear();

	m_viewPort = viewPort;
	const int width = (int)viewPort.m_vp.Width;
	const int height = (int)viewPort.m_vp.Height;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 6;
	descDepth.Format = texFormat;
	descDepth.SampleDesc.Count = isMultiSampling ? 4 : 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = D3D10_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr = renderer.GetDevice()->CreateTexture2D(&descDepth, NULL, &m_texture);
	RETV2(FAILED(hr), false);

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = DSVFormat;
	descDSV.ViewDimension = isMultiSampling ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	descDSV.Texture2DArray.MipSlice = 0;
	descDSV.Texture2DArray.FirstArraySlice = 0;
	descDSV.Texture2DArray.ArraySize = 6;
	hr = renderer.GetDevice()->CreateDepthStencilView(m_texture, &descDSV, &m_depthDSV);
	RETV2(FAILED(hr), false);

	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descSRV, sizeof(descSRV));
	descSRV.Format = SRVFormat;
	descSRV.ViewDimension = isMultiSampling ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE;
	descSRV.TextureCube.MipLevels = 1;
	descSRV.TextureCube.MostDetailedMip = 0;
	hr = renderer.GetDevice()->CreateShaderResourceView(m_texture, &descSRV, &m_depthSRV);
	RETV2(FAILED(hr), false);

	descSRV.ViewDimension = isMultiSampling ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	descSRV.Texture2DArray.MipLevels = 1;
	descSRV.Texture2DArray.FirstArraySlice = 0;
	descSRV.Texture2DArray.ArraySize = 6;
	hr = renderer.GetDevice()->CreateShaderResourceView(m_texture, &descSRV, &m_depthSRVArray);
	RETV2(FAILED(hr), false);

	return true;
}


bool cCubeDepthBuffer::Begin(cRenderer &renderer
	, const bool isClear //= true
	, const Vector4 &clearColor // = Vector4(1,1,1,1)
)
{
	SetRenderTarget(renderer);

	if (isClear)
	{
		if (renderer.ClearScene(false, clearColor))
		{
			renderer.BeginScene();
			return true;
		}
	}
	else
	{
		renderer.BeginScene();
		return true;
	}

	return false;
}


void cCubeDepthBuffer::End(cRenderer &renderer)
{
	renderer.EndScene();
	renderer.SetRenderTarget(NULL, NULL);
	renderer.m_viewPort.Bind(renderer);
	renderer.UnbindShaderAll();
}


void cCubeDepthBuffer::SetRenderTarget(cRenderer &renderer)
{
	renderer.SetRenderTargetDepth(m_depthDSV);

	const float w = m_viewPort.m_vp.Width;
	const float h = m_viewPort.m_vp.Height;
	D3D11_VIEWPORT vp[6] = { { 0, 0, w, h, 0.0f, 1.0f },{ 0, 0, w, h, 0.0f, 1.0f }
		,{ 0, 0, w, h, 0.0f, 1.0f },{ 0, 0, w, h, 0.0f, 1.0f }
		,{ 0, 0, w, h, 0.0f, 1.0f },{ 0, 0, w, h, 0.0f, 1.0f } };
	renderer.GetDevContext()->RSSetViewports(6, vp);
}


void cCubeDepthBuffer::RecoveryRenderTarget(cRenderer &renderer)
{
	renderer.SetRenderTarget(NULL, NULL);
	renderer.m_viewPort.Bind(renderer);
}


void cCubeDepthBuffer::Bind(cRenderer &renderer
	, const int stage //= 0
)
{
	renderer.GetDevContext()->PSSetShaderResources(stage, 1, &m_depthSRV);
}


void cCubeDepthBuffer::Render(cRenderer &renderer)
{
	static const char *shaderPath = "../Media/shadowmap_pointlight/depthbuffer.fxo";
	cShader11 *gbuffShader = renderer.m_shaderMgr.LoadShader(renderer, shaderPath, 0, false);
	gbuffShader->SetTechnique("Unlit");
	gbuffShader->Begin();
	gbuffShader->BeginPass(renderer, 0);

	ID3D11DeviceContext *devContext = renderer.GetDevContext();
	renderer.GetDevContext()->PSSetShaderResources(0, 1, &m_depthSRVArray);

	devContext->IASetInputLayout(NULL);
	devContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	devContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	devContext->Draw(24, 0);

	ID3D11ShaderResourceView *srvs[] = { NULL };
	devContext->PSSetShaderResources(0, 1, srvs);
}


void cCubeDepthBuffer::Clear()
{
	SAFE_RELEASE(m_texture);
	SAFE_RELEASE(m_depthSRV);
	SAFE_RELEASE(m_depthSRVArray);
	SAFE_RELEASE(m_depthDSV);
}
