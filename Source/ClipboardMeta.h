#pragma once

struct ClipboardMeta
{
    enum SourceType { None = 0, Dynamics = 1, Shift = 2, Plain = 3 };
    int source = None;
    int curveIndex = 0;

    // Dynamics display range
    float dynMinDB = -60.0f;
    float dynMaxDB = 0.0f;

    // Shift range
    float shiftMinHz = -500.0f;
    float shiftMaxHz = 500.0f;

    // Mult range
    float multMin = 0.5f;
    float multMax = 2.0f;
};
