#include "SampRenderHook.h"

#include "SampCoreDOD.h"
#include "../main.h"

#include <EGL/egl.h>
#include <dlfcn.h>
#include "shadowhook.h"

namespace
{
using EglSwapBuffersFn = EGLBoolean (*)(EGLDisplay display, EGLSurface surface);

EglSwapBuffersFn g_originalEglSwapBuffers = nullptr;
void* g_eglSwapBuffersStub = nullptr;
bool g_installAttempted = false;

EGLBoolean EglSwapBuffersHook(EGLDisplay display, EGLSurface surface)
{
    XyronCoreFrameTick();

    if (g_originalEglSwapBuffers)
    {
        return g_originalEglSwapBuffers(display, surface);
    }

    return EGL_FALSE;
}
}

extern "C" void XyronInstallEglSwapBuffersHook() noexcept
{
    if (g_installAttempted)
    {
        return;
    }
    g_installAttempted = true;

    void* egl = dlopen("libEGL.so", RTLD_NOW);
    void* symbol = egl ? dlsym(egl, "eglSwapBuffers") : dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (!symbol)
    {
        FLog("[CORE_RENDER] eglSwapBuffers symbol unavailable; Render2dStuff pipeline remains primary.");
        return;
    }

    g_eglSwapBuffersStub = shadowhook_hook_func_addr(
        symbol,
        reinterpret_cast<void*>(EglSwapBuffersHook),
        reinterpret_cast<void**>(&g_originalEglSwapBuffers));

    FLog("[CORE_RENDER] eglSwapBuffers hook %s; ImGui render path is native Render2dStuff.",
         g_eglSwapBuffersStub ? "installed" : "not-installed");
}

extern "C" bool XyronIsEglSwapBuffersHookInstalled() noexcept
{
    return g_eglSwapBuffersStub != nullptr;
}
