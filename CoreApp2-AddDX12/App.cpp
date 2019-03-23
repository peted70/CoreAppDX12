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

//uint32_t g_ClientWidth = 1280;
//uint32_t g_ClientHeight = 720;

// Set to true once the DX12 objects have been initialized.
bool g_IsInitialized = false;

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

        CoreDispatcher dispatcher = window.Dispatcher();
        dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
    }

    void SetWindow(CoreWindow const & window)
    {
		EnableDebugLayer();

		auto adapter = GetAdapter(false);
		auto device = CreateDevice(adapter);

		auto cmdQueue = CreateCommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);

		auto width = static_cast<uint32_t>(window.Bounds().Width);
		auto height = static_cast<uint32_t>(window.Bounds().Height);
		CreateSwapChain(device, cmdQueue, width, height, 3);

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
		uint32_t width, uint32_t height, uint32_t bufferCount)
	{
		com_ptr<IDXGISwapChain4> dxgiSwapChain4;
		com_ptr<IDXGIFactory4> dxgiFactory4;
		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

		check_hresult(CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), dxgiFactory4.put_void()));

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
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
		check_pointer(swapChain1.as<IDXGISwapChain4>().get());

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
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	CoreApplication::Run(make<App>());
}
