#include "Native3DTextLabels.h"

#include "game/game.h"
#include "net/netgame.h"

extern CNetGame* pNetGame;

void Native3DTextLabels::Render(ImDrawList* draw, const ImVec2& displaySize)
{
    if (!draw || !pNetGame || !pNetGame->GetTextLabelPool()) {
        return;
    }

    pNetGame->GetTextLabelPool()->RenderNative(draw, displaySize);
}
