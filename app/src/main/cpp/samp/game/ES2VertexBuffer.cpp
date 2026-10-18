//
// Created by x1y2z on 25.05.2023.
//

#include "ES2VertexBuffer.h"
#include "GTASAEngineApi.h"

void ES2VertexBuffer::InjectHooks() {
    GTASAEngineApi::BindES2VertexBufferGlobals(&ES2VertexBuffer::curCPUBuffer);
}
