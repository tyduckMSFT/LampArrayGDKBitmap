#pragma once

#include "MainPage.g.h"
#include "KDTree.h"

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
        enum class LampArrayBitmapOrientation : uint32_t
        {
            XYPlane,
            XZPlane,
            YZPlane,
        };

        struct LampArrayContext
        {
        public:
            LampArrayContext(_In_ ILampArray* lampArray) : m_lampArray(lampArray) {}
            void Initialize();

            ILampArray* GetLampArray() { return m_lampArray.get(); }
            LampArrayBitmapOrientation GetOrientation() { return m_orientation; }

        private:
            void CalculateOrientationAndBottomRightCorner();
            void FindBoundingBoxesForAllLamps();
            void FindBoundingBoxesForSelectedLamps();

            LampArrayPosition TransformToOrientation(const LampArrayPosition& position);

            wil::com_ptr_nothrow<ILampArray> m_lampArray;

            // Which plane the bitmap will render on.
            LampArrayBitmapOrientation m_orientation{};

            // Position of the bottom right corner of the LampArray boundingbox for this bitmap
            // No need for the top-left as it will always be 0,0
            LampArrayPosition m_lampArrayBottomRight{};

            // Bounding boxes for every Lamp.
            std::vector<BoundingBox> m_lampBoxes;

            // Bounding boxes for just those Lamps selected by this effect.
            // Origin is zero'd to that of the encompassing box.
            std::vector<BoundingBox> m_selectedLampBoxes;
        };

        void DisplayBitmapOnLampArrays();

        static void OnLampArrayStatusChanged(
            _In_opt_ void* context,
            LampArrayStatus currentStatus,
            LampArrayStatus previousStatus,
            _In_ ILampArray* lampArray);

        wil::srwlock m_lampArraysLock;
        _Guarded_by_(m_lampArraysLock) std::vector<std::unique_ptr<LampArrayContext>> m_lampArrays;

        LampArrayCallbackToken m_lampArrayCallbackToken{};
    };
}

namespace winrt::LampArrayGDKBitmap::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
