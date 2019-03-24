#include "pch.h"
#include "MeshNode.h"
//#include "Common\DirectXHelper.h"
//#include "BufferManager.h"
//#include "ImgUtils.h"
//#include "DxUtils.h"
//#include "ShaderCache.h"
//#include "BufferCache.h"
//#include "ModelBufferManager.h"

const char *defineLookup[] =
{
	"HAS_BASECOLORMAP",
	"HAS_NORMALMAP",
	"HAS_EMISSIVEMAP",
	"HAS_OCCLUSIONMAP",
	"HAS_METALROUGHNESSMAP"
};

MeshNode::MeshNode(int index) :
	GraphContainerNode(index)
{
}

MeshNode::~MeshNode()
{
}

void MeshNode::Initialise()
{
	GraphContainerNode::Initialise();
	CreateDeviceDependentResources();
}

void MeshNode::AfterLoad()
{
	CompileAndLoadVertexShader();
	CompileAndLoadPixelShader();
	GraphContainerNode::AfterLoad();
}

const char *normals = "NORMALS";
const char *uvs = "UV";

void MeshNode::CompileAndLoadVertexShader()
{
}

void MeshNode::CompileAndLoadPixelShader()
{
}

void MeshNode::CreateDeviceDependentResources()
{
}

void MeshNode::Draw(IGraphicsContext& context, XMMATRIX model)
{
	if (!m_loadingComplete)
		return;

	GraphContainerNode::Draw(context, model);
}

void BoundingSphereFromBoundingBox()
{

}

//void MeshNode::CreateBuffer(WinRTGLTFParser::GLTF_BufferData ^ data)
//{
//	// properties to key the buffer from.. need to be unique for a particular
//	// GLB file...
//	BufferDescriptor descriptor(data, DevResources());
//
//	auto bufferCache = ModelBufferManager::Instance().CurrentBufferCache();
//
//	auto bufferWrapper = bufferCache->FindOrCreateBuffer(descriptor);
//
//	wstring type(data->BufferDescription->BufferContentType->Data());
//	BufferWrapper bw(data, bufferWrapper->Buffer());
//	_buffers[type] = bw;
//}
//
//void MeshNode::CreateMaterial(GLTF_MaterialData ^ data)
//{
//	_material = make_shared<NodeMaterial>();
//	_material->Initialise(data);
//}
//
//void MeshNode::CreateTransform(GLTF_TransformData^ data)
//{
//	// If we are handed a matrix, just apply that, otherwise break down into scale, rotate, translate
//	// and generate the matrix from those..createbuffer
//	if (data->hasMatrix)
//	{
//		_hasMatrix = true;
//		_matrix = 
//		{
//			data->matrix[0], 
//			data->matrix[1],
//			data->matrix[2],
//			data->matrix[3],
//			data->matrix[4],
//			data->matrix[5],
//			data->matrix[6],
//			data->matrix[7],
//			data->matrix[8],
//			data->matrix[9],
//			data->matrix[10],
//			data->matrix[11],
//			data->matrix[12],
//			data->matrix[13],
//			data->matrix[14],
//			data->matrix[15]
//		};
//
//		XMStoreFloat4x4(&BufferManager::Instance().MVPBuffer().BufferData().model, XMMatrixTranspose(XMLoadFloat4x4(&_matrix)));
//	}
//	else
//	{
//		_scale = { data->scale[0], data->scale[1], data->scale[2] };
//		_translation = { data->translation[0], data->translation[1], data->translation[2] };
//
//		_rotation = { data->rotation[0], data->rotation[1], data->rotation[2], data->rotation[3] };
//
//		// Create matrix from scale
//		auto matrix = XMMatrixAffineTransformation(XMLoadFloat3(&_scale), XMLoadFloat3(&emptyVector), XMLoadFloat4(&_rotation), XMLoadFloat3(&_translation));
//
//		// Prepare to pass the updated model matrix to the shader 
//		XMStoreFloat4x4(&BufferManager::Instance().MVPBuffer().BufferData().model, XMMatrixTranspose(matrix));
//	}
//}
//
//void MeshNode::CreateTexture(WinRTGLTFParser::GLTF_TextureData ^ data)
//{
//	auto res = _material->HasTextureId(data->Idx);
//
//	// Don't want to allocate textures we have already allocated..
//	if (res)
//	{
//		// Don't need to load the image but we need to register an entry for the texture type in the
//		// textures map..
//		_material->AddTexture(res);
//		return;
//	}
//
//	Utility::Out(L"Create texture id - %d", data->Idx);
//
//	// Create texture.
//	D3D11_TEXTURE2D_DESC txtDesc = {};
//	txtDesc.MipLevels = txtDesc.ArraySize = 1;
//
//	// TODO: Fix this - understand when to use sRGB and RGB 
//	txtDesc.Format = (data->Type == 4 || data->Type == 3) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
//	txtDesc.SampleDesc.Count = 1;
//	txtDesc.Usage = D3D11_USAGE_IMMUTABLE;
//	txtDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
//
//	uint32_t width;
//	uint32_t height;
//
//	auto image = ImgUtils::LoadRGBAImage((void *)data->pSysMem, data->DataSize, width, height);
//
//	txtDesc.Width = width;
//	txtDesc.Height = height;
//
//	D3D11_SUBRESOURCE_DATA initialData = {};
//	initialData.pSysMem = image.data();
//	initialData.SysMemPitch = txtDesc.Width * sizeof(uint32_t);
//
//	ComPtr<ID3D11Texture2D> tex;
//	DX::ThrowIfFailed(
//		DevResources()->GetD3DDevice()->CreateTexture2D(&txtDesc, &initialData,
//			tex.GetAddressOf()));
//
//	ComPtr<ID3D11ShaderResourceView> textureResourceView;
//	DX::ThrowIfFailed(
//		DevResources()->GetD3DDevice()->CreateShaderResourceView(tex.Get(),
//			nullptr, textureResourceView.ReleaseAndGetAddressOf()));
//
//	// Create sampler.
//	D3D11_SAMPLER_DESC samplerDesc = {};
//	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
//	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
//	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
//	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
//	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
//	samplerDesc.MinLOD = 0;
//	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
//
//	ComPtr<ID3D11SamplerState> texSampler;
//	DX::ThrowIfFailed(DevResources()->GetD3DDevice()->CreateSamplerState(&samplerDesc, texSampler.ReleaseAndGetAddressOf()));
//
//	_material->AddTexture(data->Idx, data->Type, tex, textureResourceView, texSampler);
//}

