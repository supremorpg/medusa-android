#include "../main.h"
#include "font.h"
#include "GTASAEngineApi.h"

void CFont::Initialise() {
	GTASAEngineApi::InitialiseFont();
}

void CFont::AsciiToGxtChar(const char* ascii, uint16_t* gxt)
{
	GTASAEngineApi::AsciiToGxtChar(ascii, gxt);
}

void CFont::SetScale(float x, float y)
{
	GTASAEngineApi::SetFontScale(x, y);
}

void CFont::SetColor(uint32_t* dwColor)
{
	GTASAEngineApi::SetFontColor(dwColor);
}

void CFont::SetJustify(uint8_t justify)
{
	GTASAEngineApi::SetFontJustify(justify);
}

void CFont::SetOrientation(uint8_t orientation)
{
	GTASAEngineApi::SetFontOrientation(orientation);
}

void CFont::SetWrapX(float wrapX)
{
	GTASAEngineApi::SetFontWrapX(wrapX);
}

void CFont::SetCentreSize(float size)
{
	GTASAEngineApi::SetFontCentreSize(size);
}
void Font_SetRightJustifyWrap(float wrap)
{
	GTASAEngineApi::SetFontRightJustifyWrap(wrap);
}

void CFont::SetBackground(uint8_t bBackground, uint8_t bOnlyText)
{
	GTASAEngineApi::SetFontBackground(bBackground, bOnlyText);
}

void CFont::SetBackgroundColor(uint32_t* dwColor)
{
	GTASAEngineApi::SetFontBackgroundColor(dwColor);
}

void CFont::SetProportional(uint8_t prop)
{
	GTASAEngineApi::SetFontProportional(prop);
}

void CFont::SetDropColor(uint32_t* dwColor)
{
	GTASAEngineApi::SetFontDropColor(dwColor);
}

void CFont::SetDropShadowPosition(uint8_t pos)
{
	GTASAEngineApi::SetFontDropShadowPosition(pos);
}

void CFont::PrintString(float posX, float posY, const char* string)
{
	uint16_t gxt_string[0xFF]{};
	CFont::AsciiToGxtChar(string ? string : "", gxt_string);
	GTASAEngineApi::PrintGxtString(posX, posY, gxt_string);
	GTASAEngineApi::RenderFontBuffer();
}

void CFont::SetFontStyle(uint8_t style)
{
	GTASAEngineApi::SetFontStyle(style);
}

void CFont::SetEdge(uint8_t edge)
{
	GTASAEngineApi::SetFontEdge(edge);
}
