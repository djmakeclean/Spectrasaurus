#include "PiecewiseFunction.h"
#include <cmath>

PiecewiseFunction::PiecewiseFunction()
{
    reset();
}

void PiecewiseFunction::reset(float defaultY)
{
    points.clear();
    points.push_back(ControlPoint(0.0f, defaultY));
    points.push_back(ControlPoint(1.0f, defaultY));
    ++version;
}

float PiecewiseFunction::evaluate(float x) const
{
    x = std::clamp(x, 0.0f, 1.0f);

    // Find the two points that bracket x
    for (size_t i = 0; i < points.size() - 1; ++i)
    {
        if (x >= points[i].x && x <= points[i + 1].x)
        {
            float dx = points[i + 1].x - points[i].x;
            if (dx < 1e-8f)
                return points[i].y; // Avoid division by zero for coincident points
            float t = (x - points[i].x) / dx;
            return points[i].y + t * (points[i + 1].y - points[i].y);
        }
    }

    // Fallback (shouldn't reach here)
    return 0.0f;
}

void PiecewiseFunction::addPoint(float x, float y)
{
    x = std::clamp(x, 0.0f, 1.0f);
    // Don't clamp Y — allow values outside 0-1 for above-0dB dynamics support
    // Individual windows handle their own Y clamping as needed

    points.push_back(ControlPoint(x, y));
    sortPoints();
    ++version;
}

bool PiecewiseFunction::removePoint(int index)
{
    // Cannot remove endpoints (first and last)
    if (index <= 0 || index >= static_cast<int>(points.size()) - 1)
        return false;

    points.erase(points.begin() + index);
    ++version;
    return true;
}

void PiecewiseFunction::updatePoint(int index, float newX, float newY)
{
    if (index < 0 || index >= static_cast<int>(points.size()))
        return;

    // Don't clamp Y — allow values outside 0-1 for above-0dB dynamics support
    // Individual windows handle their own Y clamping as needed

    // Endpoints can only move vertically
    if (index == 0)
    {
        points[0].y = newY;
    }
    else if (index == static_cast<int>(points.size()) - 1)
    {
        points.back().y = newY;
    }
    else
    {
        // Interior points: keep X away from endpoints to avoid div-by-zero
        newX = std::clamp(newX, 0.001f, 0.999f);
        points[index].x = newX;
        points[index].y = newY;
        sortPoints();
    }
    ++version;
}

int PiecewiseFunction::findClosestPoint(float x, float y, float maxDistance) const
{
    int closestIndex = -1;
    float closestDistSq = maxDistance * maxDistance;

    for (size_t i = 0; i < points.size(); ++i)
    {
        float dx = points[i].x - x;
        float dy = points[i].y - y;
        float distSq = dx * dx + dy * dy;

        if (distSq < closestDistSq)
        {
            closestDistSq = distSq;
            closestIndex = static_cast<int>(i);
        }
    }

    return closestIndex;
}

void PiecewiseFunction::sortPoints()
{
    std::sort(points.begin(), points.end());
    ensureEndpoints();
}

void PiecewiseFunction::ensureEndpoints()
{
    // Ensure we always have endpoints at x=0 and x=1
    if (points.empty() || points.front().x != 0.0f)
    {
        points.insert(points.begin(), ControlPoint(0.0f, 0.0f));
    }

    if (points.empty() || points.back().x != 1.0f)
    {
        points.push_back(ControlPoint(1.0f, 0.0f));
    }
}

void PiecewiseFunction::flattenSegmentAt(float x, float y)
{
    if (points.size() < 2) return;

    x = std::clamp(x, 0.0f, 1.0f);

    // Find the segment containing x (points[i] to points[i+1])
    for (size_t i = 0; i < points.size() - 1; ++i)
    {
        if (x >= points[i].x && x <= points[i + 1].x)
        {
            float leftX = points[i].x;
            float rightX = points[i + 1].x;

            // Add new points at the segment endpoints with the flat Y value,
            // using a tiny offset so they sit just inside the segment
            float eps = std::max((rightX - leftX) * 0.001f, 1e-6f);
            addPoint(leftX + eps, y);  // increments version
            addPoint(rightX - eps, y); // increments version
            return;
        }
    }
}

void PiecewiseFunction::copyFrom(const PiecewiseFunction& other)
{
    points = other.points;
    ++version;
}

juce::var PiecewiseFunction::toVar() const
{
    juce::Array<juce::var> arr;
    for (const auto& pt : points)
    {
        juce::Array<juce::var> pair;
        pair.add(static_cast<double>(pt.x));
        pair.add(static_cast<double>(pt.y));
        arr.add(juce::var(pair));
    }
    return juce::var(arr);
}

void PiecewiseFunction::fromVar(const juce::var& v)
{
    points.clear();
    if (auto* arr = v.getArray())
    {
        for (const auto& item : *arr)
        {
            if (auto* pair = item.getArray())
            {
                if (pair->size() >= 2)
                {
                    float x = static_cast<float>(static_cast<double>((*pair)[0]));
                    float y = static_cast<float>(static_cast<double>((*pair)[1]));
                    points.push_back(ControlPoint(x, y));
                }
            }
        }
    }
    if (points.empty())
        reset(); // increments version
    else
    {
        ensureEndpoints();
        ++version;
    }
}
