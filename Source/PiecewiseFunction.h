#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>

struct ControlPoint
{
    float x; // Normalized 0.0 to 1.0
    float y; // Usually 0.0 to 1.0, but can exceed for dynamics above-0dB

    ControlPoint(float x_ = 0.0f, float y_ = 0.0f) : x(x_), y(y_) {}

    bool operator<(const ControlPoint& other) const { return x < other.x; }
};

class PiecewiseFunction
{
public:
    PiecewiseFunction();
    ~PiecewiseFunction() = default;

    // Evaluate the piecewise function at normalized x [0.0, 1.0]
    float evaluate(float x) const;

    // Add a control point (will be sorted automatically)
    void addPoint(float x, float y);

    // Remove a control point by index (endpoints cannot be removed)
    bool removePoint(int index);

    // Update a control point's position
    void updatePoint(int index, float newX, float newY);

    // Get all control points
    const std::vector<ControlPoint>& getPoints() const { return points; }

    // Find closest point to given coordinates (in normalized space)
    int findClosestPoint(float x, float y, float maxDistance) const;

    // Reset to default (flat line at given y value)
    void reset(float defaultY = 0.0f);

    // Flatten just the segment containing x by setting both bracketing points' Y to the given y
    void flattenSegmentAt(float x, float y);

    // Check if the curve is flat at a given Y value (all points have the same Y)
    bool isFlat(float y, float tolerance = 1e-6f) const
    {
        for (const auto& pt : points)
            if (std::abs(pt.y - y) > tolerance)
                return false;
        return true;
    }

    // Copy from another function
    void copyFrom(const PiecewiseFunction& other);

    // Serialization
    juce::var toVar() const;
    void fromVar(const juce::var& v);

    // Version counter — incremented on every mutation for LUT cache invalidation
    uint32_t version = 0;

private:
    std::vector<ControlPoint> points;

    void sortPoints();
    void ensureEndpoints();
};
