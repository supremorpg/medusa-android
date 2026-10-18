#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "nettransport.h"

#include <algorithm>

extern CGame* pGame;
extern CNetGame* pNetGame;

namespace {
bool IsPointInRectPadded64(float x, float y, const CRect& source, float padX, float padY)
{
    CRect rect = source;
    if (rect.left > rect.right) {
        std::swap(rect.left, rect.right);
    }
    if (rect.bottom > rect.top) {
        std::swap(rect.bottom, rect.top);
    }

    rect.left -= padX;
    rect.right += padX;
    rect.bottom -= padY;
    rect.top += padY;
    return x >= rect.left && x <= rect.right && y >= rect.bottom && y <= rect.top;
}

void LogTextDrawTouch64(const char* phase, int type, int x, int y, int id, const CRect& rect)
{
    static uint32_t s_lastLogTick = 0;
    const uint32_t now = GetTickCount();
    if (id < 0 && now - s_lastLogTick < 350) {
        return;
    }
    s_lastLogTick = now;
    FLog("[TEXTDRAW_TOUCH64] %s type=%d x=%d y=%d id=%d rect=%.1f/%.1f/%.1f/%.1f",
         phase,
         type,
         x,
         y,
         id,
         rect.left,
         rect.bottom,
         rect.right,
         rect.top);
}
}

// 0.3.7
CTextDrawPool::CTextDrawPool()
{
    for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
        m_pTextDraw[i] = nullptr;
        m_bSlotState[i] = false;
    }

    ResetTextDrawTextures();

    // ====
    m_bSelectState = false;
    m_dwHoverColor = 0;
    m_wClickedTextDrawID = 0xFFFF;
}
// 0.3.7
CTextDrawPool::~CTextDrawPool()
{
    for (int i = 0; i < MAX_TEXT_DRAWS; i++) {
        Delete(i);
    }
}
// 0.3.7
void CTextDrawPool::New(uint16_t wTextDrawID, TEXT_DRAW_TRANSMIT* pTextDrawTransmit, const char* szText)
{
    if (wTextDrawID >= MAX_TEXT_DRAWS) {
        FLog("[TEXTDRAW_GUARD64] reject-new invalid id=%u max=%u", wTextDrawID, MAX_TEXT_DRAWS);
        return;
    }

    if (m_pTextDraw[wTextDrawID]) {
        Delete(wTextDrawID);
    }

    CTextDraw* pTextDraw = new CTextDraw(pTextDrawTransmit, szText);
    if (pTextDraw == nullptr) return;

    m_pTextDraw[wTextDrawID] = pTextDraw;
    m_bSlotState[wTextDrawID] = true;
}
// 0.3.7
void CTextDrawPool::Delete(uint16_t wTextDrawID)
{
    if (wTextDrawID >= MAX_TEXT_DRAWS) {
        FLog("[TEXTDRAW_GUARD64] reject-delete invalid id=%u max=%u", wTextDrawID, MAX_TEXT_DRAWS);
        return;
    }

    if (m_pTextDraw[wTextDrawID]) {
        delete m_pTextDraw[wTextDrawID];
        m_pTextDraw[wTextDrawID] = nullptr;
        m_bSlotState[wTextDrawID] = false;
    }
}
// 0.3.7
void CTextDrawPool::Draw()
{
    for (int i = 0; i < MAX_TEXT_DRAWS; i++)
    {
        if (m_bSlotState[i]) {
            m_pTextDraw[i]->Draw();
        }
    }
}

void CTextDrawPool::DrawImage()
{
    /*for (int i = 0; i < MAX_TEXT_DRAWS; i++)
    {
        if (m_bSlotState[i]) {
            m_pTextDraw[i]->DrawImage();
        }
    }*/
}

// ========

void CTextDrawPool::SetSelectState(bool bState, uint32_t dwColor)
{
    if (bState) {
        m_bSelectState = true;
        m_dwHoverColor = (((dwColor << 16) | dwColor & 0xFF00) << 8) | (((dwColor >> 16) | dwColor & 0xFF0000) >> 8);
        m_wClickedTextDrawID = 0xFFFF;
        if (pGame) {
            pGame->DisplayHUD(false);
            if (pGame->FindPlayerPed()) {
                pGame->FindPlayerPed()->TogglePlayerControllable(false);
            }
        }
        FLog("[TEXTDRAW_SELECT64] state=on hover=0x%08x", dwColor);
    }
    else {
        m_bSelectState = false;
        m_dwHoverColor = 0;
        m_wClickedTextDrawID = 0xFFFF;
        if (pGame) {
            pGame->DisplayHUD(true);
            if (pGame->FindPlayerPed()) {
                pGame->FindPlayerPed()->TogglePlayerControllable(true);
            }
        }
        FLog("[TEXTDRAW_SELECT64] state=off");

        for (int i = 0; i < MAX_TEXT_DRAWS; i++)
        {
            if (m_bSlotState[i] && m_pTextDraw[i]) {
                CTextDraw* pTextDraw = m_pTextDraw[i];
                pTextDraw->m_bHovered = false;
                pTextDraw->m_dwHoverColor = 0;
            }
        }
    }
}

void CTextDrawPool::SendClick()
{
    if (!pNetGame || !pNetGame->GetRakClient()) {
        return;
    }

    RakNet::BitStream bsClick;
    bsClick.Write(m_wClickedTextDrawID);
    FLog("[TEXTDRAW_CLICK64] send id=%u", m_wClickedTextDrawID);
    NetTransport::Rpc(pNetGame->GetRakClient(), &RPC_ClickTextDraw, &bsClick, HIGH_PRIORITY, RELIABLE_ORDERED, 0, false, UNASSIGNED_NETWORK_ID, nullptr, "textdraw-click");
}

bool CTextDrawPool::onTouchEvent(int type, bool multi, int x, int y)
{
    if (m_bSelectState == false) return true;
    static int s_pressedTextDraw = -1;

    m_wClickedTextDrawID = 0xFFFF;

    bool hasSelectable = false;
    int hoveredId = -1;
    CRect hoveredRect;
    for (int i = 0; i < MAX_TEXT_DRAWS; i++)
    {
        if (m_bSlotState[i] && m_pTextDraw[i])
        {
            CTextDraw* pTextDraw = m_pTextDraw[i];
            pTextDraw->m_bHovered = false;
            pTextDraw->m_dwHoverColor = 0;

            if (pTextDraw->m_TextDrawData.byteSelectable)
            {
                hasSelectable = true;
                const float padX = RsGlobal ? std::max(8.0f, RsGlobal->maximumWidth * 0.006f) : 8.0f;
                const float padY = RsGlobal ? std::max(8.0f, RsGlobal->maximumHeight * 0.010f) : 8.0f;
                if (pTextDraw->m_TextDrawData.bHasRectArea &&
                    IsPointInRectPadded64((float)x, (float)y, pTextDraw->m_rectArea, padX, padY))
                {
                    hoveredId = i;
                    hoveredRect = pTextDraw->m_rectArea;
                }
            }
        }
    }

    if (hoveredId >= 0) {
        CTextDraw* pHoveredTextDraw = m_pTextDraw[hoveredId];
        if (pHoveredTextDraw) {
            pHoveredTextDraw->m_bHovered = true;
            pHoveredTextDraw->m_dwHoverColor = m_dwHoverColor;
        }

        switch (type)
        {
            case 2:
                s_pressedTextDraw = hoveredId;
                LogTextDrawTouch64("down-hit", type, x, y, hoveredId, hoveredRect);
                return false;

            case 3:
                LogTextDrawTouch64("move-hit", type, x, y, hoveredId, hoveredRect);
                return false;

            case 1:
                if (s_pressedTextDraw < 0 || s_pressedTextDraw == hoveredId) {
                    m_wClickedTextDrawID = hoveredId;
                    LogTextDrawTouch64("up-click", type, x, y, hoveredId, hoveredRect);
                    SendClick();
                    s_pressedTextDraw = -1;
                    return false;
                }
                LogTextDrawTouch64("up-hit-other", type, x, y, hoveredId, hoveredRect);
                s_pressedTextDraw = -1;
                return false;
        }
    }

    if (type == 1) {
        s_pressedTextDraw = -1;
    }
    if (hasSelectable) {
        CRect emptyRect;
        LogTextDrawTouch64("miss", type, x, y, -1, emptyRect);
    }

    return true;
}

void CTextDrawPool::SnapshotProcess()
{
    for (int i = 0; i < MAX_TEXT_DRAWS; i++)
    {
        if (m_bSlotState[i] && m_pTextDraw[i]) {
            m_pTextDraw[i]->SnapshotProcess();
        }
    }
}
