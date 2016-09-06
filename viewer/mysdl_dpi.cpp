#include "mysdl_dpi.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <ShellScalingAPI.h>
#include <comdef.h>
#pragma comment(lib, "Shcore.lib")
#endif

#include <SDL.h>

#include <string>

int MySDL_SetProcessDpiAware()
{
#ifdef _WIN32
    HRESULT hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    if (FAILED(hr))
    {
        _com_error err(hr);

#ifdef UNICODE
        int bufSize = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, err.ErrorMessage(), -1, NULL, 0, NULL, NULL);
        if (bufSize == 0)
        {
            SDL_SetError("WideCharToMultiByte error");
            return 1;
        }

        std::string s(bufSize, 0);
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, err.ErrorMessage(), -1, &s[0], bufSize, NULL, NULL))
        {
            SDL_SetError("WideCharToMultiByte error");
            return 1;
        }
        s.pop_back(); // remove extra null terminator
#else
        std::string s(err.ErrorMessage());
#endif

        SDL_SetError("SetProcessDpiAwareness: %s", s.c_str());
        return -1;
    }
#endif
    
    return 0;
}

void MySDL_GetDisplayDPI(int displayIndex, float* hdpi, float* vdpi, float* defaultDpi)
{
    static const float kSysDefaultDpi =
#ifdef __APPLE__
        72.0f;
#elif defined(_WIN32)
        96.0f;
#else
        static_assert(false, "No system default DPI set for this platform");
#endif

    if (SDL_GetDisplayDPI(displayIndex, NULL, hdpi, vdpi))
    {
        if (hdpi) *hdpi = kSysDefaultDpi;
        if (vdpi) *vdpi = kSysDefaultDpi;
    }

    if (defaultDpi) *defaultDpi = kSysDefaultDpi;
}