#include "pch.h"
#include "DX12GraphicsContext.h"

using namespace winrt::Windows;
using namespace winrt::Windows::ApplicationModel::Core;

using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;

DX12GraphicsContext::DX12GraphicsContext()
{
}

DX12GraphicsContext::~DX12GraphicsContext()
{
}

void DX12GraphicsContext::Initialise()
{
	EnableDebugLayer();

	auto adapter = GetAdapter(false);
	Device = CreateDevice(adapter);

	CommandQueue = CreateCommandQueue(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

	SwapChain = CreateSwapChain(Device, CommandQueue, 3);

	 CurrentBackBufferIndex =  SwapChain->GetCurrentBackBufferIndex();

	 RTVDescriptorHeap = CreateDescriptorHeap(Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NumFrames);
	RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(Device, SwapChain, RTVDescriptorHeap);

	for (int i = 0; i < NumFrames; ++i)
	{
		CommandAllocators[i] = CreateCommandAllocator(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	CommandList = CreateCommandList(Device,
		CommandAllocators[CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

	Fence = CreateFence(Device);
	FenceEvent = CreateEventHandle();

	IsInitialized = true;
}

void DX12GraphicsContext::Update()
{
	UpdateInternal();
}

void DX12GraphicsContext::Render()
{
	RenderInternal();
}

void DX12GraphicsContext::Resize()
{
	ResizeInternal();
}

void DX12GraphicsContext::EnableDebugLayer()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	com_ptr<ID3D12Debug> debugInterface;
	if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), debugInterface.put_void())))
	{
		debugInterface->EnableDebugLayer();
	}
#endif
}

com_ptr<IDXGIAdapter4> DX12GraphicsContext::GetAdapter(bool useWarp)
{
	com_ptr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;

#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	winrt::check_hresult(CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), dxgiFactory.put_void()));
	com_ptr<IDXGIAdapter1> dxgiAdapter1;
	com_ptr<IDXGIAdapter4> dxgiAdapter4;

	if (useWarp)
	{
		winrt::check_hresult(dxgiFactory->EnumWarpAdapter(__uuidof(IDXGIAdapter1), dxgiAdapter1.put_void()));
		dxgiAdapter4 = dxgiAdapter1.as<IDXGIAdapter4>();
	}
	else
	{
		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, dxgiAdapter1.put()) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				dxgiAdapter4 = dxgiAdapter1.as<IDXGIAdapter4>();
			}

			dxgiAdapter1 = nullptr;
		}
	}

	return dxgiAdapter4;
}

com_ptr<ID3D12Device2> DX12GraphicsContext::CreateDevice(com_ptr<IDXGIAdapter4> adapter)
{
	com_ptr<ID3D12Device2> d3d12Device2;
	check_hresult(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device2), d3d12Device2.put_void()));

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	com_ptr<ID3D12InfoQueue> pInfoQueue;
	pInfoQueue = d3d12Device2.as<ID3D12InfoQueue>();

	if (pInfoQueue)
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		check_hresult(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	return d3d12Device2;
}

com_ptr<ID3D12CommandQueue> DX12GraphicsContext::CreateCommandQueue(com_ptr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
	com_ptr<ID3D12CommandQueue> d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	check_hresult(device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), d3d12CommandQueue.put_void()));

	return d3d12CommandQueue;
}

bool DX12GraphicsContext::CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	com_ptr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), factory4.put_void())))
	{
		com_ptr<IDXGIFactory5> factory5 = factory4.as<IDXGIFactory5>();
		if (factory5)
		{
			if (FAILED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

com_ptr<IDXGISwapChain4> DX12GraphicsContext::CreateSwapChain(com_ptr<ID3D12Device2> device, com_ptr<ID3D12CommandQueue> commandQueue,
	uint32_t bufferCount)
{
	com_ptr<IDXGISwapChain4> dxgiSwapChain4;
	com_ptr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	check_hresult(CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), dxgiFactory4.put_void()));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	auto coreWindow = CoreWindow::GetForCurrentThread();
	com_ptr<IDXGISwapChain1> swapChain1;
	check_hresult(dxgiFactory4->CreateSwapChainForCoreWindow(
		commandQueue.get(),
		coreWindow.try_as<IUnknown>().get(),
		&swapChainDesc,
		nullptr,
		swapChain1.put()));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	// Not sure I need to do this for CoreWindow - think about another way... 
	// check_hresult(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
	dxgiSwapChain4 = swapChain1.as<IDXGISwapChain4>();
	check_pointer(dxgiSwapChain4.get());

	return dxgiSwapChain4;
}

com_ptr<ID3D12DescriptorHeap> DX12GraphicsContext::CreateDescriptorHeap(com_ptr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
{
	com_ptr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	check_hresult(device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), descriptorHeap.put_void()));

	return descriptorHeap;
}

void DX12GraphicsContext::UpdateRenderTargetViews(com_ptr<ID3D12Device2> device, com_ptr<IDXGISwapChain4> swapChain,
	com_ptr<ID3D12DescriptorHeap> descriptorHeap)
{
	auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < NumFrames; ++i)
	{
		com_ptr<ID3D12Resource> backBuffer;
		check_hresult(swapChain->GetBuffer(i, __uuidof(ID3D12Resource), backBuffer.put_void()));

		device->CreateRenderTargetView(backBuffer.get(), nullptr, rtvHandle);

		BackBuffers[i] = backBuffer;

		rtvHandle.Offset(rtvDescriptorSize);
	}
}

com_ptr<ID3D12CommandAllocator> DX12GraphicsContext::CreateCommandAllocator(com_ptr<ID3D12Device2> device,
	D3D12_COMMAND_LIST_TYPE type)
{
	com_ptr<ID3D12CommandAllocator> commandAllocator;
	check_hresult(device->CreateCommandAllocator(type, __uuidof(ID3D12CommandAllocator), commandAllocator.put_void()));

	return commandAllocator;
}

com_ptr<ID3D12GraphicsCommandList> DX12GraphicsContext::CreateCommandList(com_ptr<ID3D12Device2> device,
	com_ptr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	com_ptr<ID3D12GraphicsCommandList> commandList;
	check_hresult(device->CreateCommandList(0, type, commandAllocator.get(), nullptr, __uuidof(ID3D12GraphicsCommandList), commandList.put_void()));

	check_hresult(commandList->Close());

	return commandList;
}

com_ptr<ID3D12Fence> DX12GraphicsContext::CreateFence(com_ptr<ID3D12Device2> device)
{
	com_ptr<ID3D12Fence> fence;

	check_hresult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), fence.put_void()));

	return fence;
}

HANDLE DX12GraphicsContext::CreateEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

uint64_t DX12GraphicsContext::Signal(com_ptr<ID3D12CommandQueue> commandQueue, com_ptr<ID3D12Fence> fence,
	uint64_t& fenceValue)
{
	uint64_t fenceValueForSignal = ++fenceValue;
	check_hresult(commandQueue->Signal(fence.get(), fenceValueForSignal));

	return fenceValueForSignal;
}

void DX12GraphicsContext::WaitForFenceValue(com_ptr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
	std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		check_hresult(fence->SetEventOnCompletion(fenceValue, fenceEvent));
		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

void DX12GraphicsContext::Flush(com_ptr<ID3D12CommandQueue> commandQueue, com_ptr<ID3D12Fence> fence,
	uint64_t& fenceValue, HANDLE fenceEvent)
{
	uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

void DX12GraphicsContext::UpdateInternal()
{
	static uint64_t frameCounter = 0;
	static double elapsedSeconds = 0.0;
	static std::chrono::high_resolution_clock clock;
	static auto t0 = clock.now();

	frameCounter++;
	auto t1 = clock.now();
	auto deltaTime = t1 - t0;
	t0 = t1;

	elapsedSeconds += deltaTime.count() * 1e-9;
	if (elapsedSeconds > 1.0)
	{
		wchar_t buffer[500];
		auto fps = frameCounter / elapsedSeconds;
		swprintf_s(buffer, 500, L"FPS: %f\n", fps);
		OutputDebugString(buffer);

		frameCounter = 0;
		elapsedSeconds = 0.0;
	}
}

void DX12GraphicsContext::RenderInternal()
{
	auto commandAllocator = CommandAllocators[CurrentBackBufferIndex];
	auto backBuffer = BackBuffers[CurrentBackBufferIndex];

	commandAllocator->Reset();
	CommandList->Reset(commandAllocator.get(), nullptr);

	// Clear the render target.
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		CommandList->ResourceBarrier(1, &barrier);

		FLOAT clearColor[] = { 0.9f, 0.6f, 0.9f, 1.0f };
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			CurrentBackBufferIndex, RTVDescriptorSize);

		CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	}

	// Present
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		CommandList->ResourceBarrier(1, &barrier);

		ThrowIfFailed(CommandList->Close());

		ID3D12CommandList* const commandLists[] = {
			CommandList.get()
		};
		CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		UINT syncInterval = VSync ? 1 : 0;
		UINT presentFlags = TearingSupported && !VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(SwapChain->Present(syncInterval, presentFlags));

		FrameFenceValues[CurrentBackBufferIndex] = Signal(CommandQueue, Fence, FenceValue);

		CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

		WaitForFenceValue(Fence, FrameFenceValues[CurrentBackBufferIndex], FenceEvent);
	}
}

void DX12GraphicsContext::ResizeInternal()
{
	// Flush the GPU queue to make sure the swap chain's back buffers
	// are not being referenced by an in-flight command list.
	Flush(CommandQueue, Fence, FenceValue, FenceEvent);

	for (int i = 0; i < NumFrames; ++i)
	{
		// Any references to the back buffers must be released
		// before the swap chain can be resized.
		BackBuffers[i] = nullptr;
		FrameFenceValues[i] = FrameFenceValues[CurrentBackBufferIndex];
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	check_hresult(SwapChain->GetDesc(&swapChainDesc));
	auto bounds = CoreWindow::GetForCurrentThread().Bounds();
	check_hresult(SwapChain->ResizeBuffers(NumFrames, static_cast<UINT>(bounds.Width), static_cast<UINT>(bounds.Height),
		swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

	CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

	UpdateRenderTargetViews(Device, SwapChain, RTVDescriptorHeap);
}

