#pragma once

// Simple container class to describe a BoundingBox formed between the coordinates (Left, Top) and (Right, Bottom)
struct BoundingBox
{
    int32_t Left;
    int32_t Top;
    int32_t Right;
    int32_t Bottom;
};

namespace KDTree
{
    struct Point
    {
        int32_t values[2]; // values[0] is the X coordinate, values[1] is the Y coordinate
    };

    struct Data
    {
        Point point;
        size_t indexBoundingBox;
    };

    struct QueueData
    {
        size_t rangeBegin;
        size_t rangeEnd;
    };

    HRESULT GenerateAllBoundingBoxes(
        std::vector<Data>& looseNodes,
        const BoundingBox& globalBoundingBox,
        std::vector<BoundingBox>& result) noexcept;

    // Repeatedly partitions the points such that all points to the left of the median are smaller
    // and all points to the right are larger.
    // At each recursion level we swap the axis we are comparing against
    HRESULT GenerateKDTreeInPlace(
        std::vector<Data>& looseNodes,
        std::vector<QueueData>(&partitioningQueue)[2]) noexcept;

    // The k-d tree must contain at least 2 points
    HRESULT FindNearestNeighbor(
        size_t elementIndex,
        const std::vector<Data>& kdTree,
        std::vector<QueueData>(&searchQueue)[2],
        size_t& result) noexcept;

    // The k-d tree must contain at least 2 points
    HRESULT FindNeighborsWithinRadius(
        size_t elementIndex,
        int32_t searchDistanceSquared,
        const std::vector<Data>& kdTree,
        std::vector<QueueData>(&searchQueue)[2],
        std::vector<size_t>& results) noexcept;
}
