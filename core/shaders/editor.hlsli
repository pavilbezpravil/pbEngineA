#include "shared/hlslCppShared.hlsli"

cbuffer gEditorCB : DECLARE_REGISTER(b, CB_SLOT_EDITOR) {
  SEditorCB gEditor;
}

RWStructuredBuffer<uint> gUnderCursorBuffer;

void SetEntityUnderCursor(int2 pixelIdx, uint entityID) {
    if (all(pixelIdx == gEditor.cursorPixelIdx)) {
        gUnderCursorBuffer[0] = entityID; // todo: atomic add
    }
}
