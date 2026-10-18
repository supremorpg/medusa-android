#include "../main.h"
#include "game.h"
#include "util.h"
#include "RW/RenderWare.h"
#include "../gui/gui.h"
#include "../util/CUtil.h"

#include <algorithm>
#include <cmath>

extern UI* pUI;
extern CSnapShotHelper* pSnapShotHelper;

extern uintptr_t TextDrawTexture[];

namespace {
float TextDrawScaleX64()
{
    return RsGlobal ? (float)RsGlobal->maximumWidth * (1.0f / 640.0f) : 1.0f;
}

float TextDrawScaleY64()
{
    return RsGlobal ? (float)RsGlobal->maximumHeight * (1.0f / 448.0f) : 1.0f;
}

void NormalizeTextDrawRect64(CRect& rect)
{
    if (rect.left > rect.right) {
        std::swap(rect.left, rect.right);
    }
    if (rect.bottom > rect.top) {
        std::swap(rect.bottom, rect.top);
    }
}

void ExpandSmallTextDrawRect64(CRect& rect, float minWidth, float minHeight)
{
    NormalizeTextDrawRect64(rect);

    const float width = rect.right - rect.left;
    if (width < minWidth) {
        const float center = (rect.left + rect.right) * 0.5f;
        rect.left = center - minWidth * 0.5f;
        rect.right = center + minWidth * 0.5f;
    }

    const float height = rect.top - rect.bottom;
    if (height < minHeight) {
        const float center = (rect.bottom + rect.top) * 0.5f;
        rect.bottom = center - minHeight * 0.5f;
        rect.top = center + minHeight * 0.5f;
    }
}
}

// 0.3.7
CTextDraw::CTextDraw(TEXT_DRAW_TRANSMIT* pTextDrawTransmit, const char* szText)
{
    memset(&m_TextDrawData, 0, sizeof(TEXT_DRAW_DATA));

    m_TextDrawData.fLetterWidth = pTextDrawTransmit->fLetterWidth;
    m_TextDrawData.fLetterHeight = pTextDrawTransmit->fLetterHeight;

    m_TextDrawData.dwLetterColor = pTextDrawTransmit->dwLetterColor;
    m_TextDrawData.byteUnk12 = 0;
    m_TextDrawData.byteCentered = pTextDrawTransmit->byteCenter;
    m_TextDrawData.byteBox = pTextDrawTransmit->byteBox;

    m_TextDrawData.fLineWidth = pTextDrawTransmit->fLineWidth;
    m_TextDrawData.fLineHeight = pTextDrawTransmit->fLineHeight;

    m_TextDrawData.dwBoxColor = pTextDrawTransmit->dwBoxColor;
    m_TextDrawData.byteProportional = pTextDrawTransmit->byteProportional;
    m_TextDrawData.dwBackgroundColor = pTextDrawTransmit->dwBackgroundColor;
    m_TextDrawData.byteShadow = pTextDrawTransmit->byteShadow;
    m_TextDrawData.byteOutline = pTextDrawTransmit->byteOutline;
    m_TextDrawData.byteAlignLeft = pTextDrawTransmit->byteLeft;
    m_TextDrawData.byteAlignRight = pTextDrawTransmit->byteRight;
    m_TextDrawData.dwStyle = pTextDrawTransmit->byteStyle;

    m_TextDrawData.fX = pTextDrawTransmit->fX;
    m_TextDrawData.fY = pTextDrawTransmit->fY;

    m_TextDrawData.dwParam1 = 0xFFFFFFFF;
    m_TextDrawData.dwParam2 = 0xFFFFFFFF;
    m_TextDrawData.byteSelectable = pTextDrawTransmit->byteSelectable;
    m_TextDrawData.wModelID = pTextDrawTransmit->wModelID;
    m_TextDrawData.vecRot.x = pTextDrawTransmit->vecRot.x;
    m_TextDrawData.vecRot.y = pTextDrawTransmit->vecRot.y;
    m_TextDrawData.vecRot.z = pTextDrawTransmit->vecRot.z;
    m_TextDrawData.fZoom = pTextDrawTransmit->fZoom;
    m_TextDrawData.wColor1 = pTextDrawTransmit->wColor1;
    m_TextDrawData.wColor2 = pTextDrawTransmit->wColor2;
    m_TextDrawData.bHasKeyCode = false;
    m_TextDrawData.iTextureSlot = -1;
    SetText(szText);

    if (m_TextDrawData.dwStyle == 4) {
        m_TextDrawData.iTextureSlot = GetFreeTextDrawTextureSlot();
        LoadTexture();
    }

    m_TextDrawData.bHasRectArea = false;
    m_rectArea.left = 0.0f;
    m_rectArea.right = 0.0f;
    m_rectArea.top = 0.0f;
    m_rectArea.bottom = 0.0f;
    m_bHovered = false;
    m_dwHoverColor = 0;

}
// 0.3.7
CTextDraw::~CTextDraw()
{
    if (m_TextDrawData.iTextureSlot != -1 && m_TextDrawData.iTextureSlot != 0x0 && TextDrawTexture[m_TextDrawData.iTextureSlot]) RwTextureDestroy((RwTexture*)TextDrawTexture[m_TextDrawData.iTextureSlot]);
    DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);
}

uintptr_t LoadFromTxdSlot(const char* szSlot, const char* szTexture)
{
    if (!szSlot || !szTexture || strlen(szSlot) == 0 || strlen(szTexture) == 0)
    {
        FLog("LoadFromTxdSlot ERROR: invalid params");
        return 0;
    }

    // Previni crashes
    RwTexture* tex = nullptr;

    // Lista padrão de bibliotecas
    static const char* texdb[] = { 
        "samp", "mobile", "txd", "menu", "gta3", "gta_int", "player"
    };

    bool isDefaultLibrary = false;
    for (int i = 0; i < 7; i++)
    {
        if (!strcasecmp(texdb[i], szSlot))
        {
            isDefaultLibrary = true;
            break;
        }
    }

    // Se NÃO é biblioteca padrão -> tenta carregar direto do TXD especificado
    if (!isDefaultLibrary)
    {
        tex = (RwTexture*)LoadTexture(szTexture);

        if (tex)
        {
            FLog("[TD] Loaded texture '%s' from default LoadTexture()", szTexture);
            return (uintptr_t)tex;
        }
    }

    // Fallbacks inteligentes:
    char buffer[128];

    // 1. textura normal
    if (!tex)
    {
        tex = (RwTexture*)LoadTexture(szTexture);
        if (tex)
        {
            FLog("[TD] Loaded '%s' normally", szTexture);
            return (uintptr_t)tex;
        }
    }

    // 2. textura_lowercase
    std::string lower = szTexture;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (!tex)
    {
        tex = (RwTexture*)LoadTexture(lower.c_str());
        if (tex)
        {
            FLog("[TD] Loaded '%s' lowercase", lower.c_str());
            return (uintptr_t)tex;
        }
    }

    // 3. textura_uppercase
    std::string upper = szTexture;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (!tex)
    {
        tex = (RwTexture*)LoadTexture(upper.c_str());
        if (tex)
        {
            FLog("[TD] Loaded '%s' uppercase", upper.c_str());
            return (uintptr_t)tex;
        }
    }

    // 4. textura_slot (ex.: radar -> radar_hud)
    snprintf(buffer, sizeof(buffer), "%s_%s", szTexture, szSlot);
    if (!tex)
    {
        tex = (RwTexture*)LoadTexture(buffer);
        if (tex)
        {
            FLog("[TD] Loaded '%s' as '%s_%s'", szTexture, szTexture, szSlot);
            return (uintptr_t)tex;
        }
    }

    // 5. slot_textura (ex.: hud_radar)
    snprintf(buffer, sizeof(buffer), "%s_%s", szSlot, szTexture);
    if (!tex)
    {
        tex = (RwTexture*)LoadTexture(buffer);
        if (tex)
        {
            FLog("[TD] Loaded '%s' as '%s_%s'", szTexture, szSlot, szTexture);
            return (uintptr_t)tex;
        }
    }

    // 6. Load direto do DB
    if (!tex)
    {
        tex = (RwTexture*)CUtil::LoadTextureFromDB(szSlot, szTexture);
        if (tex)
        {
            FLog("[TD] Loaded '%s:%s' from DB", szSlot, szTexture);
            return (uintptr_t)tex;
        }
    }

    // NUNCA crasha — apenas dá warning
    FLog("[TD WARNING] Failed to load texture '%s' from '%s'", szTexture, szSlot);
    return 0;
}

void CTextDraw::Draw()
{
    // 🔥 Correção: garantir que snapshots sejam criados
    if (m_TextDrawData.dwStyle == 5 && m_TextDrawData.iTextureSlot == -1)
    {
        SnapshotProcess();
    }

    if (m_TextDrawData.iTextureSlot != -1) {
        return DrawTextured();
    }

    if (m_TextDrawData.dwStyle == 4) {
        return DrawTextured();
    }

    return DrawDefault();
}

void CTextDraw::DrawDefault()
{
    //CFont::AsciiToGxtChar(m_szText, m_gxtString);
    int iScreenWidth = RsGlobal->maximumWidth;
    int iScreenHeight = RsGlobal->maximumHeight;
    float fScaleScreenX = TextDrawScaleX64();
    float fScaleScreenY = TextDrawScaleY64();
    float fHorizHudScale = 1.0f / 640.0f;
    float fVertHudScale = 1.0f / 448.0f;

    float fScaleY = (float)iScreenHeight * fVertHudScale * m_TextDrawData.fLetterHeight * 0.5f;
    float fScaleX = (float)iScreenWidth * fHorizHudScale * m_TextDrawData.fLetterWidth;

    CFont::SetScale(fScaleX, fScaleY);

    if (m_bHovered)
    {
        uint32_t dwReversed = __builtin_bswap32(m_dwHoverColor | (0x000000FF));
        CFont::SetColor(&dwReversed);
    }
    else
    {
        CFont::SetColor(&m_TextDrawData.dwLetterColor);
    }

    CFont::SetJustify(0);

    if (m_TextDrawData.byteAlignRight) CFont::SetOrientation(2);
    else if (m_TextDrawData.byteCentered) CFont::SetOrientation(0);
    else CFont::SetOrientation(1);

    float fLineWidth = iScreenWidth * m_TextDrawData.fLineWidth * fHorizHudScale;
    CFont::SetWrapX(fLineWidth);

    float fLineHeight = iScreenWidth * m_TextDrawData.fLineHeight * fHorizHudScale;
    CFont::SetCentreSize(fLineHeight);

    CFont::SetBackground(m_TextDrawData.byteBox, 0);
    CFont::SetBackgroundColor(&m_TextDrawData.dwBoxColor);
    CFont::SetProportional(m_TextDrawData.byteProportional);
    CFont::SetDropColor(&m_TextDrawData.dwBackgroundColor);

    if (m_TextDrawData.byteOutline) {
        CFont::SetEdge(m_TextDrawData.byteOutline);
    }
    else {
        //CFont::SetEdge(m_TextDrawData.byteShadow);
        CFont::SetDropShadowPosition(m_TextDrawData.byteShadow);
    }

    CFont::SetFontStyle(m_TextDrawData.dwStyle);

    if (m_TextDrawData.bHasKeyCode) {
        //  CMessages_InsertPlayerControlKeysInString(m_szString);
    }

    float fUseY = iScreenHeight - ((448.0 - m_TextDrawData.fY) * (iScreenHeight * fVertHudScale));
    float fUseX = iScreenWidth - ((640.0 - m_TextDrawData.fX) * (iScreenWidth * fHorizHudScale));
    const bool hasText = m_szText[0] != '\0';
    if (hasText) {
        CFont::PrintString(fUseX, fUseY, (const char*)m_szText);
    }

    CFont::SetEdge(0);

    const float fTextSizeX = m_TextDrawData.fLineWidth * fScaleScreenX;
    const float fTextSizeY = m_TextDrawData.fLineHeight * fScaleScreenY;
    CRect rect;
    if (m_TextDrawData.byteAlignRight)
    {
        rect.left = fTextSizeX;
        rect.right = fUseX;
        rect.bottom = fUseY;
        rect.top = fUseY + fTextSizeY;
    }
    else if (m_TextDrawData.byteCentered)
    {
        rect.left = fUseX - (std::fabs(fTextSizeX) * 0.5f);
        rect.right = fUseX + (std::fabs(fTextSizeX) * 0.5f);
        rect.bottom = fUseY;
        rect.top = fUseY + fTextSizeY;
    }
    else
    {
        rect.left = fUseX;
        rect.right = fTextSizeX;
        rect.bottom = fUseY;
        rect.top = fUseY + fTextSizeY;
    }

    const float minClickWidth = std::max(36.0f * fScaleScreenX, std::fabs(fScaleX) * 8.0f);
    const float minClickHeight = std::max(18.0f * fScaleScreenY, std::fabs(fScaleY) * 2.4f);
    ExpandSmallTextDrawRect64(rect, minClickWidth, minClickHeight);

    rect.left -= 5.0f * fScaleScreenX;
    rect.right += 5.0f * fScaleScreenX;
    rect.bottom -= 10.0f * fScaleScreenY;
    rect.top += 4.0f * fScaleScreenY;
    NormalizeTextDrawRect64(rect);

    m_rectArea = rect;
    m_TextDrawData.bHasRectArea = true;
}

void CTextDraw::DrawTextured()
{
    float scaleX = TextDrawScaleX64();
    float scaleY = TextDrawScaleY64();

    m_rectArea.left = m_TextDrawData.fX * scaleX;
    m_rectArea.bottom = m_TextDrawData.fY * scaleY;
    m_rectArea.right = (m_TextDrawData.fX + m_TextDrawData.fLineWidth) * scaleX;
    m_rectArea.top = (m_TextDrawData.fY + m_TextDrawData.fLineHeight) * scaleY;
    NormalizeTextDrawRect64(m_rectArea);
    m_TextDrawData.bHasRectArea = true;

    // Keep selectable image textdraws clickable even when the texture slot failed.
    if (m_TextDrawData.iTextureSlot < 0 || m_TextDrawData.iTextureSlot >= 200) {
        FLog("Warning: Invalid textdraw texture slot %d", m_TextDrawData.iTextureSlot);
        return;
    }

    static float uv_reflected[8] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f };

    static float uv_normal[8] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f };

    if (m_bHovered) {
        if(TextDrawTexture[m_TextDrawData.iTextureSlot] != 0)DrawTextureUV(TextDrawTexture[m_TextDrawData.iTextureSlot], &m_rectArea, m_dwHoverColor,
                                                                           m_TextDrawData.dwStyle == 5 ? uv_reflected : uv_normal);
    }
    else {
        if(TextDrawTexture[m_TextDrawData.iTextureSlot] != 0)DrawTextureUV(TextDrawTexture[m_TextDrawData.iTextureSlot], &m_rectArea, m_TextDrawData.dwLetterColor,
                                                                           m_TextDrawData.dwStyle == 5 ? uv_reflected : uv_normal);
    }
}

void CTextDraw::SetText(const char* szText)
{
    memset(m_szText, 0, 800);
    strncpy(m_szText, szText, 800);
    m_szText[800] = 0;

    if (m_TextDrawData.dwStyle == 4 && m_TextDrawData.iTextureSlot != -1)
    {
        DestroyTextDrawTexture(m_TextDrawData.iTextureSlot);
        LoadTexture();
    }
}

void CTextDraw::LoadTexture()
{
    char txdname[64 + 1];
    memset(txdname, 0, sizeof(txdname));
    char texturename[64 + 1];
    memset(texturename, 0, sizeof(texturename));

    char* szTexture = strchr(m_szText, ':');
    if (szTexture == nullptr) return;

    if (strlen(m_szText) < 64 && strchr(m_szText, '\\') == nullptr && strchr(m_szText, '/') == nullptr)
    {
        strncpy(txdname, m_szText, (size_t)(szTexture - m_szText));
        strcpy(texturename, ++szTexture);

        if (m_TextDrawData.iTextureSlot != -1 && m_TextDrawData.iTextureSlot >= 0 && m_TextDrawData.iTextureSlot < 200)
        {
            uintptr_t pTexture = ::LoadFromTxdSlot(txdname, texturename);
            if (pTexture)
            {
                TextDrawTexture[m_TextDrawData.iTextureSlot] = pTexture;
            }
            else
            {
                FLog("Warning: Failed to load textdraw texture '%s:%s'", txdname, texturename);
            }
        }
    }
}

#include "Models/ModelInfo.h"
void CTextDraw::SnapshotProcess()
{
    if (m_TextDrawData.dwStyle != 5 || m_TextDrawData.iTextureSlot != -1) {
        return;
    }

    uintptr_t snapshot = 0;

    // PED MODEL
    if (IsValidPedModel(m_TextDrawData.wModelID))
    {
        /*if (!IsExistinfoForModel(m_TextDrawData.wModelID))
            m_TextDrawData.wModelID = 0;*/

        snapshot = (uintptr_t)pSnapShotHelper->CreatePedSnapShot(
                m_TextDrawData.wModelID,
                m_TextDrawData.dwBackgroundColor,
                &m_TextDrawData.vecRot,
                m_TextDrawData.fZoom);
    }
        // VEHICLE MODEL
    else if (m_TextDrawData.wModelID >= 400 && m_TextDrawData.wModelID <= 611)
    {
        snapshot = (uintptr_t)pSnapShotHelper->CreateVehicleSnapShot(
                m_TextDrawData.wModelID,
                m_TextDrawData.dwBackgroundColor,
                &m_TextDrawData.vecRot,
                m_TextDrawData.fZoom,
                m_TextDrawData.wColor1,
                m_TextDrawData.wColor2
        );
    }
        // OBJECT MODEL
    else
    {
        if (!CModelInfo::GetModelInfo(m_TextDrawData.wModelID))
            m_TextDrawData.wModelID = 18631;

        snapshot = (uintptr_t)pSnapShotHelper->CreateObjectSnapShot(
                    m_TextDrawData.wModelID,
                    m_TextDrawData.dwBackgroundColor,
                    &m_TextDrawData.vecRot,
                    m_TextDrawData.fZoom
        );
    }

    if (snapshot)
    {
        m_TextDrawData.iTextureSlot = GetFreeTextDrawTextureSlot();
        if (m_TextDrawData.iTextureSlot >= 0 && m_TextDrawData.iTextureSlot < 200)
        {
            TextDrawTexture[m_TextDrawData.iTextureSlot] = snapshot;
        }
        else
        {
            FLog("Warning: No free textdraw texture slot available");
        }
    }
}
