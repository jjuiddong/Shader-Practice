//
// 2018-01-26, jjuiddong
// Cube DepthStencil Buffer for ShadowMap
// DepthBuffer Array
//
#pragma once


namespace graphic
{

	struct eDepthBufferType {
		enum Enum { ARRAY, CUBE };
	};


	class cDepthBufferArray
	{
	public:		
		cDepthBufferArray();
		virtual ~cDepthBufferArray();

		bool Create(cRenderer &renderer
			, const eDepthBufferType::Enum type
			, const cViewport viewPort
			, const int arrayCount = 1
			, const bool isMultiSampling = true
			, const DXGI_FORMAT texFormat = DXGI_FORMAT_R32_TYPELESS
			, const DXGI_FORMAT SRVFormat = DXGI_FORMAT_R32_FLOAT
			, const DXGI_FORMAT DSVFormat = DXGI_FORMAT_D32_FLOAT
		);

		bool Begin(cRenderer &renderer
			, const bool isClear = true
			, const Vector4 &clearColor = Vector4(1, 1, 1, 1));
		void End(cRenderer &renderer);
		void Render(cRenderer &renderer);
		void SetRenderTarget(cRenderer &renderer);
		void RecoveryRenderTarget(cRenderer &renderer);
		void Bind(cRenderer &renderer, const int stage = 0);
		void Clear();


	public:
		eDepthBufferType::Enum m_type;
		cViewport m_viewPort;
		int m_arraySize;
		ID3D11Texture2D *m_texture;
		ID3D11ShaderResourceView *m_depthSRV;
		ID3D11ShaderResourceView *m_depthSRVArray; // for debugging
		ID3D11DepthStencilView *m_depthDSV;
	};

}
