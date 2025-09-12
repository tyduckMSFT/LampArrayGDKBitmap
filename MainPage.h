#pragma once

#include "MainPage.g.h"

namespace winrt::LampArrayGDKBitmap::implementation
{
    struct MainPage : MainPageT<MainPage>
    {
        MainPage();
        ~MainPage();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

    private:
        static void OnLampArrayStatusChanged(
            _In_opt_ void* context,
            LampArrayStatus currentStatus,
            LampArrayStatus previousStatus,
            _In_ ILampArray* lampArray);

        wil::srwlock m_lampArraysLock;
        _Guarded_by_(m_lampArraysLock) std::vector<wil::com_ptr_nothrow<ILampArray>> m_lampArrays;

        LampArrayCallbackToken m_lampArrayCallbackToken{};
    };
}

namespace winrt::LampArrayGDKBitmap::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
