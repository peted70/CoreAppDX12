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
	void EnableDebugLayer();
	com_ptr<IDXGIAdapter4> GetAdapter(bool useWarp);

	com_ptr<ID3D12Device2> CreateDevice(com_ptr<IDXGIAdapter4> adapter);
	com_ptr<ID3D12CommandQueue> CreateCommandQueue(com_ptr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	bool CheckTearingSupport();
	com_ptr<IDXGISwapChain4> CreateSwapChain(com_ptr<ID3D12Device2> device, com_ptr<ID3D12CommandQueue> commandQueue,
		uint32_t bufferCount);
	com_ptr<ID3D12DescriptorHeap> CreateDescriptorHeap(com_ptr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
	void UpdateRenderTargetViews(com_ptr<ID3D12Device2> device, com_ptr<IDXGISwapChain4> swapChain,
		com_ptr<ID3D12DescriptorHeap> descriptorHeap);

	com_ptr<ID3D12CommandAllocator> CreateCommandAllocator(com_ptr<ID3D12Device2> device,
		D3D12_COMMAND_LIST_TYPE type);
	com_ptr<ID3D12GraphicsCommandList> CreateCommandList(com_ptr<ID3D12Device2> device,
		com_ptr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type);

	com_ptr<ID3D12Fence> CreateFence(com_ptr<ID3D12Device2> device);
	HANDLE CreateEventHandle();

	uint64_t Signal(com_ptr<ID3D12CommandQueue> commandQueue, com_ptr<ID3D12Fence> fence,
		uint64_t& fenceValue);
	void WaitForFenceValue(com_ptr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
		std::chrono::milliseconds duration);
	void Flush(com_ptr<ID3D12CommandQueue> commandQueue, com_ptr<ID3D12Fence> fence,
		uint64_t& fenceValue, HANDLE fenceEvent);
	void UpdateInternal();
	void RenderInternal();
	void ResizeInternal();

	// The number of swap chain back buffers.
	static const uint8_t NumFrames = 3;
	com_ptr<ID3D12Resource> BackBuffers[NumFrames];

	// Use WARP adapter
	bool UseWarp = false;

	// Set to true once the DX12 objects have been initialized.
	bool IsInitialized = false;

	// By default, enable V-Sync.
	// Can be toggled with the V key.
	bool VSync = false;
	bool TearingSupported = false;

	// By default, use windowed mode.
	// Can be toggled with the Alt+Enter or F11
	bool g_Fullscreen = false;

	// Synchronization objects
	com_ptr<ID3D12Fence> Fence;
	uint64_t FenceValue = 0;
	uint64_t FrameFenceValues[NumFrames] = {};
	HANDLE FenceEvent;

	com_ptr<ID3D12Device2> Device;
	com_ptr<ID3D12CommandQueue> CommandQueue;
	com_ptr<IDXGISwapChain4> SwapChain;
	com_ptr<ID3D12GraphicsCommandList> CommandList;
	com_ptr<ID3D12CommandAllocator> CommandAllocators[NumFrames];
	com_ptr<ID3D12DescriptorHeap> RTVDescriptorHeap;

	UINT RTVDescriptorSize;
	UINT CurrentBackBufferIndex;
};

