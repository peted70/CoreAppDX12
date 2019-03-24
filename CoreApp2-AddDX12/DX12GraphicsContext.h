#pragma once
#include <pch.h>
#include "IGraphicsContext.h"

using namespace winrt;

class DX12GraphicsContext :
	public IGraphicsContext
{
public:
	DX12GraphicsContext();
	~DX12GraphicsContext();

	// Inherited via IGraphicsContext
	virtual void Initialise() override;
	virtual void Update() override;
	virtual void Render() override;
	virtual void Resize() override;

private:
	// The number of swap chain back buffers.
	static const uint8_t g_NumFrames = 3;
	static com_ptr<ID3D12Resource> g_BackBuffers[g_NumFrames];

	// Use WARP adapter
	bool g_UseWarp = false;

	// Set to true once the DX12 objects have been initialized.
	bool g_IsInitialized = false;

	// By default, enable V-Sync.
	// Can be toggled with the V key.
	bool g_VSync = false;
	bool g_TearingSupported = false;

	// By default, use windowed mode.
	// Can be toggled with the Alt+Enter or F11
	bool g_Fullscreen = false;

	// Synchronization objects
	com_ptr<ID3D12Fence> g_Fence;
	uint64_t g_FenceValue = 0;
	uint64_t g_FrameFenceValues[g_NumFrames] = {};
	HANDLE g_FenceEvent;

	com_ptr<ID3D12Device2> g_Device;
	com_ptr<ID3D12CommandQueue> g_CommandQueue;
	com_ptr<IDXGISwapChain4> g_SwapChain;
	com_ptr<ID3D12GraphicsCommandList> g_CommandList;
	com_ptr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
	com_ptr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
	UINT g_RTVDescriptorSize;
	UINT g_CurrentBackBufferIndex;
};

