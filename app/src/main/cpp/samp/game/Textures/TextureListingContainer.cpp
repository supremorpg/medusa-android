//
// Created by x1y2z on 21.11.2023.
//

#include "TextureListingContainer.h"
#include "../GTASAEngineApi.h"

RwRaster *TextureListingContainer::CreateRaster(const TextureDatabaseEntry *forEntry) {
    return GTASAEngineApi::CreateTextureListingRasterNative(this, forEntry);
}
