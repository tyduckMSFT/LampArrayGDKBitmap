#include "pch.h"
#include "KDTree.h"

HRESULT KDTree::GenerateAllBoundingBoxes(
    std::vector<Data>& kdTree,
    const BoundingBox& globalBoundingBox,
    std::vector<BoundingBox>& result) noexcept
{
    result.clear();
    const size_t kdTreeSize = kdTree.size();

    if (kdTreeSize == 0)
    {
        return S_OK;
    }

    std::vector<QueueData> kdTreeScratchMemory[2];
    std::vector<size_t> neighborsWithinRange;

    try
    {
        result.resize(kdTreeSize);
        kdTreeScratchMemory[0].reserve(kdTreeSize);
        kdTreeScratchMemory[1].reserve(kdTreeSize);
        neighborsWithinRange.reserve(kdTreeSize);
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }

    const BoundingBox infiniteBoundingBox = {
        std::numeric_limits<int32_t>::lowest(),
        std::numeric_limits<int32_t>::lowest(),
        std::numeric_limits<int32_t>::max(),
        std::numeric_limits<int32_t>::max() };

    for (size_t i = 0; i < kdTreeSize; ++i)
    {
        result[i] = infiniteBoundingBox;
    }

    if (kdTreeSize > 1)
    {
        KDTree::GenerateKDTreeInPlace(kdTree, kdTreeScratchMemory);


        // On the first pass find the nearest neighbor, and assume that they will intersect with each other
        for (size_t i = 0; i < kdTreeSize; ++i)
        {
            const KDTree::Data& e = kdTree[i];
            const KDTree::Point& ePoint = e.point;

            size_t nearestNeighborIndex;
            RETURN_IF_FAILED(KDTree::FindNearestNeighbor(i, kdTree, kdTreeScratchMemory, nearestNeighborIndex));
            const KDTree::Point& nearestNeighbor = kdTree[nearestNeighborIndex].point;

            // The nearest neighbor is the closest point we can intersect with
            const int32_t deltaX = std::abs(ePoint.values[0] - nearestNeighbor.values[0]) / 2;
            const int32_t deltaY = std::abs(ePoint.values[1] - nearestNeighbor.values[1]) / 2;

            int32_t nudgedDeltaX;
            int32_t nudgedDeltaY;

            if ((deltaX == 0) && (deltaY == 0))
            {
                // Handle the degenerate case
                nudgedDeltaX = 1;
                nudgedDeltaY = 1;
            }
            else if ((deltaX == 0) || (deltaY == 0))
            {
                // If we are exactly perpendicular to our nearest neighbor, just form a square
                const int32_t distance = deltaX + deltaY;
                nudgedDeltaX = distance;
                nudgedDeltaY = distance;
            }
            else
            {
                // Otherwise form a rectangle
                nudgedDeltaX = deltaX;
                nudgedDeltaY = deltaY;
            }

            result[e.indexBoundingBox] = {
                e.point.values[0] - nudgedDeltaX,
                e.point.values[1] - nudgedDeltaY,
                e.point.values[0] + nudgedDeltaX,
                e.point.values[1] + nudgedDeltaY };
        }

        // On the second pass try to expand any rectangle into squares based on the existing bounding boxes
        for (size_t i = 0; i < kdTreeSize; ++i)
        {
            const KDTree::Data& e = kdTree[i];
            const KDTree::Point& ePoint = e.point;
            BoundingBox& eBoundingBox = result[e.indexBoundingBox];

            // Find the larger bound, and then try to expand the skinnier bound to it
            const int32_t deltaX = eBoundingBox.Right - eBoundingBox.Left;
            const int32_t deltaY = eBoundingBox.Bottom - eBoundingBox.Top;

            if (deltaX < deltaY)
            {
                const int32_t deltaY2 = deltaY * 2;
                const int32_t deltaY2Squared = deltaY2 * deltaY2;
                RETURN_IF_FAILED(KDTree::FindNeighborsWithinRadius(i, deltaY2Squared, kdTree, kdTreeScratchMemory, neighborsWithinRange));

                int32_t leftCollision = std::numeric_limits<int32_t>::lowest();
                int32_t rightCollision = std::numeric_limits<int32_t>::max();
                for (size_t neighborIndex : neighborsWithinRange)
                {
                    const KDTree::Data& neighbor = kdTree[neighborIndex];
                    const KDTree::Point& neighborPoint = neighbor.point;
                    const BoundingBox& neighborBoundingBox = result[neighbor.indexBoundingBox];

                    if ((eBoundingBox.Top <= neighborBoundingBox.Bottom) && (neighborBoundingBox.Bottom <= eBoundingBox.Top))
                    {
                        if (neighborPoint.values[0] < ePoint.values[0])
                        {
                            leftCollision = std::max(leftCollision, neighborBoundingBox.Right);
                        }
                        else
                        {
                            rightCollision = std::min(rightCollision, neighborBoundingBox.Left);
                        }
                    }
                }

                const int64_t closestCollision = std::min(
                    static_cast<int64_t>(ePoint.values[0]) - static_cast<int64_t>(leftCollision),
                    static_cast<int64_t>(rightCollision) - static_cast<int64_t>(ePoint.values[0]));
                const int32_t newDeltaX = static_cast<int32_t>(std::min<int64_t>(closestCollision, deltaY / 2));
                eBoundingBox.Left = ePoint.values[0] - newDeltaX;
                eBoundingBox.Right = ePoint.values[0] + newDeltaX;
            }
            else if (deltaY < deltaX)
            {
                const int32_t deltaX2 = deltaY * 2;
                const int32_t deltaX2Squared = deltaX2 * deltaX2;
                RETURN_IF_FAILED(KDTree::FindNeighborsWithinRadius(i, deltaX2Squared, kdTree, kdTreeScratchMemory, neighborsWithinRange));

                int32_t topCollision = std::numeric_limits<int32_t>::lowest();
                int32_t bottomCollision = std::numeric_limits<int32_t>::max();
                for (size_t neighborIndex : neighborsWithinRange)
                {
                    const KDTree::Data& neighbor = kdTree[neighborIndex];
                    const KDTree::Point& neighborPoint = neighbor.point;
                    const BoundingBox& neighorBoundingBox = result[neighbor.indexBoundingBox];

                    if ((eBoundingBox.Left <= neighorBoundingBox.Right) && (neighorBoundingBox.Right <= eBoundingBox.Left))
                    {
                        if (neighborPoint.values[1] < ePoint.values[1])
                        {
                            topCollision = std::max(topCollision, neighorBoundingBox.Bottom);
                        }
                        else
                        {
                            bottomCollision = std::min(bottomCollision, neighorBoundingBox.Top);
                        }
                    }
                }

                const int64_t closestCollision = std::min(
                    static_cast<int64_t>(ePoint.values[1]) - static_cast<int64_t>(topCollision),
                    static_cast<int64_t>(bottomCollision) - static_cast<int64_t>(ePoint.values[1]));
                const int32_t newDeltaY = static_cast<int32_t>(std::min<int64_t>(closestCollision, deltaX / 2));
                eBoundingBox.Top = ePoint.values[1] - newDeltaY;
                eBoundingBox.Bottom = ePoint.values[1] + newDeltaY;
            }
        }
    }

    // Clamp things that spilled over (also handles the edge case of 1 item)
    for (auto& e : result)
    {
        e.Left = std::max(e.Left, globalBoundingBox.Left);
        e.Top = std::max(e.Top, globalBoundingBox.Top);
        e.Right = std::min(e.Right, globalBoundingBox.Right);
        e.Bottom = std::min(e.Bottom, globalBoundingBox.Bottom);
    }

    return S_OK;
}

HRESULT KDTree::GenerateKDTreeInPlace(
    std::vector<Data>& looseNodes,
    std::vector<QueueData>(&partitioningQueue)[2]) noexcept
{
    const size_t nodeCount = looseNodes.size();
    if (nodeCount < 2)
    {
        // Already sorted
        return S_OK;
    }

    partitioningQueue[0].clear();
    partitioningQueue[1].clear();

    try
    {
        partitioningQueue[0].push_back({ 0, nodeCount });

        unsigned int recursionDepth = 0;
        while (!partitioningQueue[0].empty() || !partitioningQueue[1].empty())
        {
            const unsigned int currentAxis = recursionDepth & 1;
            const unsigned int otherAxis = currentAxis ^ 1;

            while (!partitioningQueue[currentAxis].empty())
            {
                const QueueData& e = partitioningQueue[currentAxis].back();
                const size_t diff = e.rangeEnd - e.rangeBegin;

                if (diff > 1)
                {
                    const size_t median = e.rangeBegin + (diff / 2);

                    std::nth_element(
                        looseNodes.begin() + e.rangeBegin,
                        looseNodes.begin() + median,
                        looseNodes.begin() + e.rangeEnd,
                        [currentAxis](const Data& lhs, const Data& rhs)
                        {
                            return lhs.point.values[currentAxis] < rhs.point.values[currentAxis];
                        });

                    partitioningQueue[otherAxis].push_back({ e.rangeBegin, median });
                    if (median + 1 < e.rangeEnd)
                    {
                        partitioningQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                    }
                }

                partitioningQueue[currentAxis].pop_back();
            }
            ++recursionDepth;
        }
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

static int32_t DistanceSquared(
    const KDTree::Point& a,
    const KDTree::Point& b) noexcept
{
    const int32_t diffX = a.values[0] - b.values[0];
    const int32_t diffY = a.values[1] - b.values[1];

    return diffX * diffX + diffY * diffY;
}

static size_t FindParent(
    const size_t elementIndex,
    const std::vector<KDTree::Data>& kdTree) noexcept
{
    const size_t nodeCount = kdTree.size();

    size_t parentIndex = nodeCount;
    size_t rangeBegin = 0;
    size_t rangeEnd = nodeCount;

    while (rangeBegin < rangeEnd)
    {
        const size_t diff = rangeEnd - rangeBegin;
        const size_t median = rangeBegin + (diff / 2);

        if (median == elementIndex)
        {
            break;
        }
        else if (elementIndex < median)
        {
            rangeEnd = median;
        }
        else
        {
            rangeBegin = median + 1;
        }

        parentIndex = median;
    }

    return parentIndex;
}

HRESULT KDTree::FindNearestNeighbor(
    const size_t elementIndex,
    const std::vector<Data>& kdTree,
    std::vector<QueueData>(&searchQueue)[2],
    size_t& result) noexcept
{
    const size_t nodeCount = kdTree.size();
    if (nodeCount < 2)
    {
        return E_INVALIDARG;
    }

    size_t nearestNeighborCandidate = 0;

    {
        const size_t median = kdTree.size() / 2;

        if (elementIndex != median)
        {
            nearestNeighborCandidate = FindParent(elementIndex, kdTree);
        }
        else
        {
            // For the root take the left child
            nearestNeighborCandidate = median / 2;
        }
    }

    const Point& elementPoint = kdTree[elementIndex].point;

    int32_t distanceToBeatSquared = DistanceSquared(elementPoint, kdTree[nearestNeighborCandidate].point);

    searchQueue[0].clear();
    searchQueue[1].clear();

    try
    {
        searchQueue[0].push_back({ 0, nodeCount });

        unsigned int recursionDepth = 0;
        while (!searchQueue[0].empty() || !searchQueue[1].empty())
        {
            const unsigned int currentAxis = recursionDepth & 1;
            const unsigned int otherAxis = currentAxis ^ 1;
            while (!searchQueue[currentAxis].empty())
            {
                const QueueData& e = searchQueue[currentAxis].back();
                const size_t diff = e.rangeEnd - e.rangeBegin;

                // Just do a linear search if we are close enough
                // 16 makes sure we're only checking a couple cache lines,
                // and has shown to improve performance.
                if (diff < 16)
                {
                    if (elementIndex < e.rangeBegin || elementIndex > e.rangeEnd)
                    {
                        for (size_t i = e.rangeBegin; i < e.rangeEnd; ++i)
                        {
                            const KDTree::Point& e = kdTree[i].point;
                            const int32_t distanceSquared = DistanceSquared(elementPoint, e);
                            if (distanceSquared < distanceToBeatSquared)
                            {
                                distanceToBeatSquared = distanceSquared;
                                nearestNeighborCandidate = i;
                            }
                        }
                    }
                    else
                    {
                        for (size_t i = e.rangeBegin; i < elementIndex; ++i)
                        {
                            const KDTree::Point& e = kdTree[i].point;
                            const int32_t distanceSquared = DistanceSquared(elementPoint, e);
                            if (distanceSquared < distanceToBeatSquared)
                            {
                                distanceToBeatSquared = distanceSquared;
                                nearestNeighborCandidate = i;
                            }
                        }

                        for (size_t i = elementIndex + 1; i < e.rangeEnd; ++i)
                        {
                            const KDTree::Point& e = kdTree[i].point;
                            const int32_t distanceSquared = DistanceSquared(elementPoint, e);
                            if (distanceSquared < distanceToBeatSquared)
                            {
                                distanceToBeatSquared = distanceSquared;
                                nearestNeighborCandidate = i;
                            }
                        }
                    }
                }
                else
                {
                    const size_t median = e.rangeBegin + (diff / 2);
                    if (elementIndex != median)
                    {
                        const Point& medianPoint = kdTree[median].point;

                        const int32_t distanceSquared = DistanceSquared(elementPoint, medianPoint);

                        const int32_t axisDistance = elementPoint.values[currentAxis] - medianPoint.values[currentAxis];
                        const int32_t axisDistanceSquared = axisDistance * axisDistance;

                        if (distanceSquared < distanceToBeatSquared)
                        {
                            distanceToBeatSquared = distanceSquared;
                            nearestNeighborCandidate = median;
                        }

                        if (diff > 1)
                        {
                            if (medianPoint.values[currentAxis] < elementPoint.values[currentAxis])
                            {
                                // See if we can avoid checking the left half
                                if (axisDistanceSquared < distanceToBeatSquared)
                                {
                                    searchQueue[otherAxis].push_back({ e.rangeBegin, median });
                                }

                                if (median + 1 < e.rangeEnd)
                                {
                                    searchQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                                }
                            }
                            else
                            {
                                searchQueue[otherAxis].push_back({ e.rangeBegin, median });

                                // See if we can avoid checking the right half
                                if (axisDistanceSquared < distanceToBeatSquared)
                                {
                                    if (median + 1 < e.rangeEnd)
                                    {
                                        searchQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // Consider both halves
                        if (diff > 1)
                        {
                            searchQueue[otherAxis].push_back({ e.rangeBegin, median });
                            if (median + 1 < e.rangeEnd)
                            {
                                searchQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                            }
                        }
                    }
                }

                searchQueue[currentAxis].pop_back();
            }
            ++recursionDepth;
        }
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }

    result = nearestNeighborCandidate;

    return S_OK;
}


// Does the same as FindNearestNeighbor, except instead of
// keeping track of distanceToBeatSquared & updating nearestNeighborCandidate
// it pushes the neighbor into result if they are within searchDistanceSquared
HRESULT KDTree::FindNeighborsWithinRadius(
    size_t elementIndex,
    int32_t searchDistanceSquared,
    const std::vector<Data>& kdTree,
    std::vector<QueueData>(&searchQueue)[2],
    std::vector<size_t>& results) noexcept
{
    const size_t nodeCount = kdTree.size();
    if (nodeCount < 2)
    {
        return E_INVALIDARG;
    }

    results.clear();

    const Point& elementPoint = kdTree[elementIndex].point;

    searchQueue[0].clear();
    searchQueue[1].clear();

    try
    {
        searchQueue[0].push_back({ 0, nodeCount });

        unsigned int recursionDepth = 0;
        while (!searchQueue[0].empty() || !searchQueue[1].empty())
        {
            const unsigned int currentAxis = recursionDepth & 1;
            const unsigned int otherAxis = currentAxis ^ 1;
            while (!searchQueue[currentAxis].empty())
            {
                const QueueData& e = searchQueue[currentAxis].back();
                const size_t diff = e.rangeEnd - e.rangeBegin;

                // Just do a linear search if we are close enough
                // 16 makes sure we're only checking a couple cache lines,
                // and has shown to improve performance.
                if (diff < 16)
                {
                    if (elementIndex < e.rangeBegin || elementIndex > e.rangeEnd)
                    {
                        for (size_t i = e.rangeBegin; i < e.rangeEnd; ++i)
                        {
                            const KDTree::Point& e = kdTree[i].point;
                            const int32_t distanceSquared = DistanceSquared(elementPoint, e);
                            if (distanceSquared < searchDistanceSquared)
                            {
                                results.push_back(i);
                            }
                        }
                    }
                    else
                    {
                        for (size_t i = e.rangeBegin; i < elementIndex; ++i)
                        {
                            const KDTree::Point& e = kdTree[i].point;
                            const int32_t distanceSquared = DistanceSquared(elementPoint, e);
                            if (distanceSquared < searchDistanceSquared)
                            {
                                results.push_back(i);
                            }
                        }

                        for (size_t i = elementIndex + 1; i < e.rangeEnd; ++i)
                        {
                            const KDTree::Point& e = kdTree[i].point;
                            const int32_t distanceSquared = DistanceSquared(elementPoint, e);
                            if (distanceSquared < searchDistanceSquared)
                            {
                                results.push_back(i);
                            }
                        }
                    }
                }
                else
                {
                    const size_t median = e.rangeBegin + (diff / 2);
                    if (elementIndex != median)
                    {
                        const Point& medianPoint = kdTree[median].point;

                        const int32_t distanceSquared = DistanceSquared(elementPoint, medianPoint);

                        const int32_t axisDistance = elementPoint.values[currentAxis] - medianPoint.values[currentAxis];
                        const int32_t axisDistanceSquared = axisDistance * axisDistance;

                        if (distanceSquared < searchDistanceSquared)
                        {
                            results.push_back(median);
                        }

                        if (diff > 1)
                        {
                            if (medianPoint.values[currentAxis] < elementPoint.values[currentAxis])
                            {
                                // See if we can avoid checking the left half
                                if (axisDistanceSquared < searchDistanceSquared)
                                {
                                    searchQueue[otherAxis].push_back({ e.rangeBegin, median });
                                }

                                if (median + 1 < e.rangeEnd)
                                {
                                    searchQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                                }
                            }
                            else
                            {
                                searchQueue[otherAxis].push_back({ e.rangeBegin, median });

                                // See if we can avoid checking the right half
                                if (axisDistanceSquared < searchDistanceSquared)
                                {
                                    if (median + 1 < e.rangeEnd)
                                    {
                                        searchQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // Consider both halves
                        if (diff > 1)
                        {
                            searchQueue[otherAxis].push_back({ e.rangeBegin, median });
                            if (median + 1 < e.rangeEnd)
                            {
                                searchQueue[otherAxis].push_back({ median + 1, e.rangeEnd });
                            }
                        }
                    }
                }

                searchQueue[currentAxis].pop_back();
            }
            ++recursionDepth;
        }
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}
