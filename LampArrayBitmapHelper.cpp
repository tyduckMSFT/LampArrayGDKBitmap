#include "pch.h"
#include "LampArrayBitmapHelper.h"

const uint32_t c_metersToMillimetersConversion = 1000;

void LampArrayBitmapHelper::Initialize()
{
    //  In this example, all Lamps of the LampArray will be used.
    m_selectedLampIndices.resize(m_lampArray->GetLampCount());
    std::iota(m_selectedLampIndices.begin(), m_selectedLampIndices.end(), 0);

    CalculateOrientationAndBottomRightCorner();
    FindBoundingBoxesForAllLamps();
    FindBoundingBoxesForSelectedLamps();
}

void LampArrayBitmapHelper::CalculateOrientationAndBottomRightCorner()
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

void LampArrayBitmapHelper::FindBoundingBoxesForAllLamps()
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

void LampArrayBitmapHelper::FindBoundingBoxesForSelectedLamps()
{
    m_selectedLampBoxes.reserve(m_selectedLampIndices.size());

    // Find corresponding selected positions.
    for (auto i = 0u; i < m_selectedLampIndices.size(); i++)
    {
        m_selectedLampBoxes.push_back(m_lampBoxes[m_selectedLampIndices[i]]);
    }

    // Determine width/height of the selection.
    BoundingBox selectedEncompassingBox = m_selectedLampBoxes[0];
    for (size_t i = 1; i < m_selectedLampBoxes.size(); i++)
    {
        if (m_selectedLampBoxes[i].Left < selectedEncompassingBox.Left)
        {
            selectedEncompassingBox.Left = m_selectedLampBoxes[i].Left;
        }

        if (m_selectedLampBoxes[i].Top < selectedEncompassingBox.Top)
        {
            selectedEncompassingBox.Top = m_selectedLampBoxes[i].Top;
        }

        if (m_selectedLampBoxes[i].Right > selectedEncompassingBox.Right)
        {
            selectedEncompassingBox.Right = m_selectedLampBoxes[i].Right;
        }

        if (m_selectedLampBoxes[i].Bottom > selectedEncompassingBox.Bottom)
        {
            selectedEncompassingBox.Bottom = m_selectedLampBoxes[i].Bottom;
        }
    }

    m_selectedEncompassingBoxWidth = static_cast<uint32_t>(
        selectedEncompassingBox.Right - selectedEncompassingBox.Left);
    m_selectedEncompassingBoxHeight = static_cast<uint32_t>(
        selectedEncompassingBox.Bottom - selectedEncompassingBox.Top);

    // Zero all selected positions to the new origin.
    for (size_t i = 0; i < m_selectedLampBoxes.size(); i++)
    {
        m_selectedLampBoxes[i].Left -= selectedEncompassingBox.Left;
        m_selectedLampBoxes[i].Top -= selectedEncompassingBox.Top;

        m_selectedLampBoxes[i].Right -= selectedEncompassingBox.Left;
        m_selectedLampBoxes[i].Bottom -= selectedEncompassingBox.Top;
    }
}

LampArrayPosition LampArrayBitmapHelper::TransformToOrientation(const LampArrayPosition& position)
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
