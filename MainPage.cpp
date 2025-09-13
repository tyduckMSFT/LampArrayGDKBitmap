#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::LampArrayGDKBitmap::implementation
{
    MainPage::MainPage()
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

        THROW_IF_FAILED(RegisterLampArrayStatusCallback(
            OnLampArrayStatusChanged,
            LampArrayEnumerationKind::Async,
            this,
            &m_lampArrayCallbackToken));
    }

    MainPage::~MainPage()
    {
        UnregisterLampArrayCallback(m_lampArrayCallbackToken, 0);
    }

    int32_t MainPage::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void MainPage::MyProperty(int32_t /* value */)
    {
        throw hresult_not_implemented();
    }

    void MainPage::ClickHandler(IInspectable const&, RoutedEventArgs const&)
    {
        myButton().Content(box_value(L"Clicked"));
    }

    void MainPage::OnLampArrayStatusChanged(
        _In_ void* context,
        LampArrayStatus currentStatus,
        LampArrayStatus previousStatus,
        _In_ ILampArray* lampArray)
    {
        auto mainPage = reinterpret_cast<MainPage*>(context);
        
        bool wasConnected = (previousStatus & LampArrayStatus::Connected) == LampArrayStatus::Connected;
        bool isConnected = (currentStatus & LampArrayStatus::Connected) == LampArrayStatus::Connected;

        if (wasConnected != isConnected)
        {
            auto lock = mainPage->m_lampArraysLock.lock_exclusive();
            auto& lampArrays = mainPage->m_lampArrays;

            if (isConnected)
            {
                auto iter = std::find_if(lampArrays.begin(), lampArrays.end(),
                    [&](const std::unique_ptr<LampArrayBitmapHelper>& ptr)
                    {
                        return lampArray == ptr->GetLampArray();
                    });

                if (iter == lampArrays.end())
                {
                    auto bitmapHelper = std::make_unique<LampArrayBitmapHelper>(lampArray);
                    bitmapHelper->Initialize();

                    lampArrays.push_back(std::move(bitmapHelper));

                    auto redColor = LampArrayColor{ 0xFF, 0, 0, 0xFF };
                    lampArray->SetColor(redColor);
                }
            }
            else
            {
                lampArrays.erase(
                    std::remove_if(lampArrays.begin(), lampArrays.end(), 
                        [&](const std::unique_ptr<LampArrayBitmapHelper>& ptr)
                        {
                            return lampArray == ptr->GetLampArray();
                        }),
                    lampArrays.end());
            }
        }
    }

    void MainPage::DisplayBitmapOnLampArrays()
    {
        auto lock = m_lampArraysLock.lock_exclusive();

        for (const auto& lampArrayContext : m_lampArrays)
        {
            auto lampArray = lampArrayContext->GetLampArray();
        }
    }
}
