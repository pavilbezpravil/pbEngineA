#include "shared/hlslCppShared.hlsli"
#include "math.hlsli"

cbuffer gEditorCB : DECLARE_REGISTER(b, CB_SLOT_EDITOR) {
  SEditorCB gEditor;
}

RWStructuredBuffer<uint> gUnderCursorBuffer; // : DECLARE_REGISTER(u, SRV_SLOT_UNDER_CURSOR_BUFFER);

void SetEntityUnderCursor(int2 pixelIdx, uint entityID) {
    if (all(pixelIdx == gEditor.cursorPixelIdx) && entityID != UINT_MAX) {
        gUnderCursorBuffer[0] = entityID; // todo: atomic add
    }
}
