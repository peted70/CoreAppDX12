#include "pch.h"

using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;

using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;

struct App : implements<App, IFrameworkViewSource, 
							 IFrameworkView>
{
    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &)
    {
		EnableDebugLayer();

		auto adapter = GetAdapter(false);
		auto device = CreateDevice(adapter);

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
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	Windows::ApplicationModel::Core::CoreApplication::Run(make<App>());
}
