#include "pch.h"
#include "IGraphicsContext.h"
#include "Graphics/DX12/DX12GraphicsContext.h"

#include <boost/di.hpp>

namespace di = boost::di;

using namespace std;
using namespace winrt;

using namespace winrt::Windows;
using namespace winrt::Windows::ApplicationModel::Core;

using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;

struct App : implements<App, IFrameworkViewSource, 
							 IFrameworkView>
{
	unique_ptr<IGraphicsContext> _graphicsContext;
	di::injector<IGraphicsContext *> _injector = di::make_injector(
		di::bind<IGraphicsContext>().to<DX12GraphicsContext>()
	);

	App()
	{
	}

    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &view)
    {

    }

    void Load(hstring const&)
    {
		_graphicsContext.reset(_injector.create<IGraphicsContext*>());
		_graphicsContext->Initialise();
	}

    void Uninitialize()
    {
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        window.Activate();

		while (true)
		{
			// Process events incoming to the window.
			window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			_graphicsContext->Update();
			_graphicsContext->Render();
		}
    }

    void SetWindow(CoreWindow const & window)
    {
		window.SizeChanged([&](auto && ...)
			{
				_graphicsContext->Resize();
				_graphicsContext->Render();
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
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	CoreApplication::Run(make<App>());
}
