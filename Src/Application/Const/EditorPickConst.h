#pragma once
// Editor object manipulation constants (mouse picking / drag / spawn / copy).
namespace EditorPickConst
{
    // Ray-sphere pick radius in world units (click tolerance around an object).
    constexpr float PickRadius = 6.0f;

    // Offset applied to a duplicated object so the copy does not overlap exactly.
    constexpr float CopyOffset = 5.0f;

    // Spawn distance in front of the editor camera when creating at current view.
    constexpr float SpawnDist = 18.0f;

    // Half size of the selection marker box drawn around the picked object.
    constexpr float MarkerHalf = 3.0f;

    // Length of each gizmo axis handle (world units).
    constexpr float GizmoLength = 12.0f;

    // Click tolerance (distance from ray to axis) for grabbing a gizmo axis.
    constexpr float GizmoPickRadius = 1.5f;

    // Half size of the small handle box drawn at each axis tip.
    constexpr float GizmoTipHalf = 0.8f;

    // Minimum move distance to record an undo step (avoids zero-length moves).
    constexpr float MoveEpsilon = 0.001f;

    // Default grid step (world units) for snap mode.
    constexpr float DefaultSnapSize = 1.0f;
}
