#pragma once

#include "KDTree.h"

enum class LampArrayBitmapOrientation : uint32_t
{
    XYPlane,
    XZPlane,
    YZPlane,
};

struct LampArrayBitmapHelper
{
public:
    LampArrayBitmapHelper(_In_ ILampArray* lampArray) : m_lampArray(lampArray) {}
    void Initialize();

    ILampArray* GetLampArray() { return m_lampArray.get(); }
    LampArrayBitmapOrientation GetOrientation() { return m_orientation; }

private:
    void CalculateOrientationAndBottomRightCorner();
    void FindBoundingBoxesForAllLamps();
    void FindBoundingBoxesForSelectedLamps();

    LampArrayPosition TransformToOrientation(const LampArrayPosition& position);

    wil::com_ptr_nothrow<ILampArray> m_lampArray;

    // Which Lamps will be used to display the bitmap. In this example,
    // all Lamps of the LampArray will be used.
    std::vector<uint32_t> m_selectedLampIndices;

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

    // Width/Height of smallest box encompassing all bounding boxes selected by this effect.
    int32_t m_selectedEncompassingBoxWidth{};
    int32_t m_selectedEncompassingBoxHeight{};
};