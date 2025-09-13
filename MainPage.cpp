#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

const uint32_t c_metersToMillimetersConversion = 1000;

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
        FindBoundingBoxesForAllLamps();
    }

    void MainPage::LampArrayContext::CalculateOrientationAndBottomRightCorner()
    {
        LampArrayPosition boundingBox{};
        m_lampArray->GetBoundingBox(&boundingBox);

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

    void MainPage::LampArrayContext::FindBoundingBoxesForAllLamps()
    {
        auto lampCount = m_lampArray->GetLampCount();
        if (lampCount == 0) { return; }

        std::vector<KDTree::Data> looseKdTreeNodes;
        looseKdTreeNodes.resize(lampCount);

        for (auto i = 0u; i < lampCount; i++)
        {
            wil::com_ptr_nothrow<ILampInfo> lampInfo;
            THROW_IF_FAILED(m_lampArray->GetLampInfo(i, &lampInfo));

            LampArrayPosition position3D{};
            lampInfo->GetPosition(&position3D);

            // All positions are in meters, convert to millimeters
            LampArrayPosition position2D = TransformToOrientation(position3D);

            position2D.xInMeters *= c_metersToMillimetersConversion;
            position2D.yInMeters *= c_metersToMillimetersConversion;

            // Push in the loose data nodes into the k-d tree, still needs to be generated
            KDTree::Data data{};
            data.point.values[0] = static_cast<int32_t>(position2D.xInMeters);
            data.point.values[1] = static_cast<int32_t>(position2D.yInMeters);
            data.indexBoundingBox = i;
            looseKdTreeNodes[i] = data;
        }

        // Makes sure that all the bounding boxes are clamped to the published device limits
        const BoundingBox globalBoundingBox = {
            0,
            0,
            static_cast<int32_t>(m_lampArrayBottomRight.xInMeters),
            static_cast<int32_t>(m_lampArrayBottomRight.yInMeters) };

        KDTree::GenerateAllBoundingBoxes(looseKdTreeNodes, globalBoundingBox, m_lampBoxes);
    }

    void MainPage::LampArrayContext::FindBoundingBoxesForSelectedLamps()
    {
    }

    LampArrayPosition MainPage::LampArrayContext::TransformToOrientation(const LampArrayPosition& position)
    {
        LampArrayPosition ret{};
        switch (m_orientation)
        {
        case LampArrayBitmapOrientation::YZPlane:
            ret.xInMeters = position.yInMeters;
            ret.yInMeters = position.zInMeters;
            break;
        case LampArrayBitmapOrientation::XZPlane:
            ret.xInMeters = position.xInMeters;
            ret.yInMeters = position.zInMeters;
            break;
        case LampArrayBitmapOrientation::XYPlane:
            ret.xInMeters = position.xInMeters;
            ret.yInMeters = position.yInMeters;
            break;
        }
        return ret;
    }
}
