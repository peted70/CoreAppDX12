#include "pch.h"

using namespace winrt;

using namespace winrt::Windows;
using namespace winrt::Windows::ApplicationModel::Core;

using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;

// The number of swap chain back buffers.
const uint8_t g_NumFrames = 3;
com_ptr<ID3D12Resource> g_BackBuffers[g_NumFrames];

// Use WARP adapter
bool g_UseWarp = false;

// Set to true once the DX12 objects have been initialized.
bool g_IsInitialized = false;

// By default, enable V-Sync.
// Can be toggled with the V key.
bool g_VSync = true;
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

struct App : implements<App, IFrameworkViewSource, 
							 IFrameworkView>
{
    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &view)
    {

    }

    void Load(hstring const&)
    {
    }

    void Uninitialize()
    {
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();

		Update();
		Render();

        CoreDispatcher dispatcher = window.Dispatcher();
        dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    }

    void SetWindow(CoreWindow const & window)
    {
		EnableDebugLayer();

		auto adapter = GetAdapter(false);
		g_Device = CreateDevice(adapter);

		g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

		g_SwapChain = CreateSwapChain(g_Device, g_CommandQueue, 3);

		g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

		g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
		g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);

		for (int i = 0; i < g_NumFrames; ++i)
		{
			g_CommandAllocators[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		}
		g_CommandList = CreateCommandList(g_Device,
			g_CommandAllocators[g_CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

		g_Fence = CreateFence(g_Device);
		g_FenceEvent = CreateEventHandle();

		g_IsInitialized = true;

		window.SizeChanged([&](auto && ...)
			{
				Resize();
				Render();
			});

        window.PointerPressed({ this, &App::OnPointerPressed });
        window.PointerMoved({ this, &App::OnPointerMoved });

        window.PointerReleased([&](auto && ...)
        {
        });
    }

    void OnPointerPressed(IInspectable const &, PointerEventArgs const & args)
    {
    }

    void OnPointerMoved(IInspectable const &, PointerEventArgs const & args)
    {
    }

	void EnableDebugLayer()
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

	com_ptr<IDXGIAdapter4> GetAdapter(bool useWarp)
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

	com_ptr<ID3D12Device2> CreateDevice(com_ptr<IDXGIAdapter4> adapter)
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

	com_ptr<ID3D12CommandQueue> CreateCommandQueue(com_ptr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
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

	bool CheckTearingSupport()
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

	com_ptr<IDXGISwapChain4> CreateSwapChain(com_ptr<ID3D12Device2> device, com_ptr<ID3D12CommandQueue> commandQueue, 
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

	com_ptr<ID3D12DescriptorHeap> CreateDescriptorHeap(com_ptr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
	{
		com_ptr<ID3D12DescriptorHeap> descriptorHeap;

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = numDescriptors;
		desc.Type = type;

		check_hresult(device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), descriptorHeap.put_void()));

		return descriptorHeap;
	}

	void UpdateRenderTargetViews(com_ptr<ID3D12Device2> device, com_ptr<IDXGISwapChain4> swapChain, 
		com_ptr<ID3D12DescriptorHeap> descriptorHeap)
	{
		auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

		for (int i = 0; i < g_NumFrames; ++i)
		{
			com_ptr<ID3D12Resource> backBuffer;
			check_hresult(swapChain->GetBuffer(i, __uuidof(ID3D12Resource), backBuffer.put_void()));

			device->CreateRenderTargetView(backBuffer.get(), nullptr, rtvHandle);

			g_BackBuffers[i] = backBuffer;

			rtvHandle.Offset(rtvDescriptorSize);
		}
	}

	com_ptr<ID3D12CommandAllocator> CreateCommandAllocator(com_ptr<ID3D12Device2> device,
		D3D12_COMMAND_LIST_TYPE type)
	{
		com_ptr<ID3D12CommandAllocator> commandAllocator;
		check_hresult(device->CreateCommandAllocator(type, __uuidof(ID3D12CommandAllocator), commandAllocator.put_void()));

		return commandAllocator;
	}

	com_ptr<ID3D12GraphicsCommandList> CreateCommandList(com_ptr<ID3D12Device2> device,
		com_ptr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
	{
		com_ptr<ID3D12GraphicsCommandList> commandList;
		check_hresult(device->CreateCommandList(0, type, commandAllocator.get(), nullptr, __uuidof(ID3D12GraphicsCommandList), commandList.put_void()));

		check_hresult(commandList->Close());

		return commandList;
	}

	com_ptr<ID3D12Fence> CreateFence(com_ptr<ID3D12Device2> device)
	{
		com_ptr<ID3D12Fence> fence;

		check_hresult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), fence.put_void()));

		return fence;
	}

	HANDLE CreateEventHandle()
	{
		HANDLE fenceEvent;

		fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fenceEvent && "Failed to create fence event.");

		return fenceEvent;
	}

	uint64_t Signal(com_ptr<ID3D12CommandQueue> commandQueue, com_ptr<ID3D12Fence> fence,
		uint64_t& fenceValue)
	{
		uint64_t fenceValueForSignal = ++fenceValue;
		check_hresult(commandQueue->Signal(fence.get(), fenceValueForSignal));

		return fenceValueForSignal;
	}

	void WaitForFenceValue(com_ptr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
		std::chrono::milliseconds duration = std::chrono::milliseconds::max())
	{
		if (fence->GetCompletedValue() < fenceValue)
		{
			check_hresult(fence->SetEventOnCompletion(fenceValue, fenceEvent));
			::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
		}
	}

	void Flush(com_ptr<ID3D12CommandQueue> commandQueue, com_ptr<ID3D12Fence> fence,
		uint64_t& fenceValue, HANDLE fenceEvent)
	{
		uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
		WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
	}

	void Update()
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

	void Render()
	{
		auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
		auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];

		commandAllocator->Reset();
		g_CommandList->Reset(commandAllocator.get(), nullptr);

		// Clear the render target.
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer.get(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

			g_CommandList->ResourceBarrier(1, &barrier);

			FLOAT clearColor[] = { 0.9f, 0.6f, 0.9f, 1.0f };
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				g_CurrentBackBufferIndex, g_RTVDescriptorSize);

			g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		}

		// Present
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				backBuffer.get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			g_CommandList->ResourceBarrier(1, &barrier);

			ThrowIfFailed(g_CommandList->Close());

			ID3D12CommandList* const commandLists[] = {
				g_CommandList.get()
			};
			g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

			UINT syncInterval = g_VSync ? 1 : 0;
			UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
			ThrowIfFailed(g_SwapChain->Present(syncInterval, presentFlags));

			g_FrameFenceValues[g_CurrentBackBufferIndex] = Signal(g_CommandQueue, g_Fence, g_FenceValue);

			g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

			WaitForFenceValue(g_Fence, g_FrameFenceValues[g_CurrentBackBufferIndex], g_FenceEvent);
		}
	}

	void Resize()
	{
		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

		for (int i = 0; i < g_NumFrames; ++i)
		{
			// Any references to the back buffers must be released
			// before the swap chain can be resized.
			g_BackBuffers[i] = nullptr;
			g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		check_hresult(g_SwapChain->GetDesc(&swapChainDesc));
		auto bounds = CoreWindow::GetForCurrentThread().Bounds();
		check_hresult(g_SwapChain->ResizeBuffers(g_NumFrames, bounds.Width, bounds.Height, 
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);
	}
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	CoreApplication::Run(make<App>());
}
