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
                    [&](const std::unique_ptr<LampArrayContext>& ptr)
                    {
                        return lampArray == ptr->GetLampArray();
                    });

                if (iter == lampArrays.end())
                {
                    auto lampArrayContext = std::make_unique<LampArrayContext>(lampArray);
                    lampArrayContext->Initialize();

                    lampArrays.push_back(std::move(lampArrayContext));

                    auto redColor = LampArrayColor{ 0xFF, 0, 0, 0xFF };
                    lampArray->SetColor(redColor);
                }
            }
            else
            {
                lampArrays.erase(
                    std::remove_if(lampArrays.begin(), lampArrays.end(), 
                        [&](const std::unique_ptr<LampArrayContext>& ptr)
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

    void MainPage::LampArrayContext::Initialize()
    {
        CalculateOrientationAndBottomRightCorner();
    }

    void MainPage::LampArrayContext::CalculateOrientationAndBottomRightCorner()
    {
        LampArrayPosition boundingBox{};
        m_lampArray->GetBoundingBox(&boundingBox);

        const uint32_t c_metersToMillimetersConversion = 1000;

        boundingBox.xInMeters *= c_metersToMillimetersConversion;
        boundingBox.yInMeters *= c_metersToMillimetersConversion;
        boundingBox.zInMeters *= c_metersToMillimetersConversion;

        float xyPlane = boundingBox.xInMeters * boundingBox.yInMeters;
        float yzPlane = boundingBox.zInMeters * boundingBox.yInMeters;
        float xzPlane = boundingBox.xInMeters * boundingBox.zInMeters;

        if (xyPlane >= yzPlane && xyPlane >= xzPlane)
        {
            m_orientation = LampArrayBitmapOrientation::XYPlane;
        }
        else if (yzPlane >= xzPlane && yzPlane >= xyPlane)
        {
            m_orientation = LampArrayBitmapOrientation::YZPlane;
        }
        else if (xzPlane >= yzPlane && xzPlane >= xyPlane)
        {
            m_orientation = LampArrayBitmapOrientation::XZPlane;
        }
        else
        {
            // All else fails assume XY.
            m_orientation = LampArrayBitmapOrientation::XYPlane;
        }

        m_lampArrayBottomRight = TransformToOrientation(boundingBox);
    }

    LampArrayPosition MainPage::LampArrayContext::TransformToOrientation(const LampArrayPosition& position)
    {
        return LampArrayPosition();
    }
}
