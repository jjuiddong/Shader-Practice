//
// 2017-12-31, jjuiddong
// Diferred Shading Graphic Buffer
// HLSL Programming CookBook sample rewrite
//
#pragma once


struct sCbGBuffer
{
	XMVECTOR perspectiveValue;
	XMMATRIX invView;
};


class cGBuffer
{
public:
	cGBuffer();
	virtual ~cGBuffer();

	bool Create(graphic::cRenderer &renderer, const UINT width, const UINT height );
	bool Begin(graphic::cRenderer &renderer);
	void End(graphic::cRenderer &renderer);
	void PrepareForUnpack(graphic::cRenderer &renderer);
	void Render(graphic::cRenderer &renderer);
	void Clear();


public:
	// GBuffer textures
	ID3D11Texture2D* m_DepthStencilRT;
	ID3D11Texture2D* m_ColorSpecIntensityRT;
	ID3D11Texture2D* m_NormalRT;
	ID3D11Texture2D* m_SpecPowerRT;

	// GBuffer render views
	ID3D11DepthStencilView* m_DepthStencilDSV;
	ID3D11DepthStencilView* m_DepthStencilReadOnlyDSV;
	ID3D11RenderTargetView* m_ColorSpecIntensityRTV;
	ID3D11RenderTargetView* m_NormalRTV;
	ID3D11RenderTargetView* m_SpecPowerRTV;

	// GBuffer shader resource views
	ID3D11ShaderResourceView* m_DepthStencilSRV;
	ID3D11ShaderResourceView* m_ColorSpecIntensitySRV;
	ID3D11ShaderResourceView* m_NormalSRV;
	ID3D11ShaderResourceView* m_SpecPowerSRV;

	ID3D11DepthStencilState *m_DepthStencilState;

	graphic::cConstantBuffer<sCbGBuffer> m_cbGBuffer;
};
