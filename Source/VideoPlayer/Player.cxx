/*
Copyright (c) 2023 Americus Maximus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Player.hxx"

#include <dshow.h>
#include <stdarg.h>
#include <stdio.h>

#define PLAYER_WINDOW_TITLE_NAME "Video Player"

#define MAX_BUFFER_SIZE 256

struct State
{
    HWND HWND;
    BOOL IsQuit;

    IGraphBuilder* GraphBuilder;
    IMediaControl* MediaControl;
    IVideoWindow* VideoWindow;
    IMediaEventEx* MediaEvent;

    struct
    {
        char AudioFileName[MAX_BUFFER_SIZE];
        WCHAR AudioWideFileName[MAX_BUFFER_SIZE];

        char VideoFileName[MAX_BUFFER_SIZE];
        WCHAR VideoWideFileName[MAX_BUFFER_SIZE];
    } Buffer;

    IBaseFilter* VideoFilter;
    IBaseFilter* VideoSplitter;
    IBaseFilter* VideoDecoder;
    IBaseFilter* VideoRenderer;

    IBaseFilter* AudioFilter;
    IBaseFilter* AudioSplitter;
    IBaseFilter* AudioDecoder;
    IBaseFilter* AudioRenderer;
} State;

void StringFormat(char* buffer, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, MAX_BUFFER_SIZE, format, args);
    va_end(args);
}

void Quit(const char* format, ...)
{
    char buffer[MAX_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, MAX_BUFFER_SIZE, format, args);
    va_end(args);

    MessageBoxA(GetLastActivePopup(State.HWND), buffer, PLAYER_WINDOW_TITLE_NAME, MB_ICONERROR | MB_OK);
}

void StringToWideString(LPWSTR wide, LPCSTR string)
{
    MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS | MB_PRECOMPOSED, string, -1, wide, MAX_BUFFER_SIZE);
}

void Initialize(void)
{
    if (State.GraphBuilder->AddSourceFilter(State.Buffer.VideoWideFileName, L"MPEG-I Stream Video Filter", &State.VideoFilter) != S_OK)
    {
        Quit("Unable to set video source filter."); return;
    }

    CoCreateInstance(CLSID_MPEG1Splitter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&State.VideoSplitter);
    CoCreateInstance(CLSID_CMpegVideoCodec, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&State.VideoDecoder);
    CoCreateInstance(CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&State.VideoRenderer);

    State.GraphBuilder->AddFilter(State.VideoSplitter, NULL);
    State.GraphBuilder->AddFilter(State.VideoDecoder, NULL);
    State.GraphBuilder->AddFilter(State.VideoRenderer, NULL);

    if (State.GraphBuilder->AddSourceFilter(State.Buffer.AudioWideFileName, L"MPEG-I Stream Audio Filter", &State.AudioFilter) != S_OK)
    {
        Quit("Unable to set audio source filter."); return;
    }

    CoCreateInstance(CLSID_MPEG1Splitter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&State.AudioSplitter);
    CoCreateInstance(CLSID_CMpegAudioCodec, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&State.AudioDecoder);
    CoCreateInstance(CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&State.AudioRenderer);

    State.GraphBuilder->AddFilter(State.AudioSplitter, NULL);
    State.GraphBuilder->AddFilter(State.AudioDecoder, NULL);
    State.GraphBuilder->AddFilter(State.AudioRenderer, NULL);
}

void Configure(void)
{
    {
        IPin* videoFilterOutPin = NULL;
        if (FAILED(State.VideoFilter->FindPin(L"Output", &videoFilterOutPin)))
        {
            Quit("Unable to find video filter output pin."); return;
        }

        IPin* videoSplitterInPin = NULL;
        if (FAILED(State.VideoSplitter->FindPin(L"Input", &videoSplitterInPin)))
        {
            Quit("Unable to find video filter input pin."); return;
        }

        if (FAILED(State.GraphBuilder->Connect(videoFilterOutPin, videoSplitterInPin)))
        {
            Quit("Unable to connect video filter and splitter pins."); return;
        }
    }

    {
        IPin* videoSplitterOutPin = NULL;
        if (FAILED(State.VideoSplitter->FindPin(L"Video", &videoSplitterOutPin)))
        {
            Quit("Unable to find video splitter output pin."); return;
        }

        IPin* videoDecoderInPin = NULL;
        if (FAILED(State.VideoDecoder->FindPin(L"In", &videoDecoderInPin)))
        {
            Quit("Unable to find video decoder input pin."); return;
        }

        if (FAILED(State.GraphBuilder->Connect(videoSplitterOutPin, videoDecoderInPin)))
        {
            Quit("Unable to connect video splitter and decoder pins."); return;
        }
    }

    {
        IPin* videoDecoderOutPin = NULL;
        if (FAILED(State.VideoDecoder->FindPin(L"Out", &videoDecoderOutPin)))
        {
            Quit("Unable to find video decoder output pin."); return;
        }

        IPin* videoRendererInPin = NULL;
        if (FAILED(State.VideoRenderer->FindPin(L"In", &videoRendererInPin)))
        {
            Quit("Unable to find video renderer input pin."); return;
        }

        if (FAILED(State.GraphBuilder->Connect(videoDecoderOutPin, videoRendererInPin)))
        {
            Quit("Unable to connect video decoder and renderer pins."); return;
        }
    }

    {
        IPin* AudioFilterOutPin = NULL;
        if (FAILED(State.AudioFilter->FindPin(L"Output", &AudioFilterOutPin)))
        {
            Quit("Unable to find audio filter output pin."); return;
        }

        IPin* AudioSplitterInPin = NULL;
        if (FAILED(State.AudioSplitter->FindPin(L"Input", &AudioSplitterInPin)))
        {
            Quit("Unable to find audio splitter input pin."); return;
        }

        if (FAILED(State.GraphBuilder->Connect(AudioFilterOutPin, AudioSplitterInPin)))
        {
            Quit("Unable to connect audio filter and splitter pins."); return;
        }
    }

    {
        IPin* AudioSplitterOutPin = NULL;
        if (FAILED(State.AudioSplitter->FindPin(L"Audio", &AudioSplitterOutPin)))
        {
            Quit("Unable to find audio splitter output pin."); return;
        }

        IPin* AudioDecoderInPin = NULL;
        if (FAILED(State.AudioDecoder->FindPin(L"In", &AudioDecoderInPin)))
        {
            Quit("Unable to find audio decoder input pin."); return;
        }

        if (FAILED(State.GraphBuilder->Connect(AudioSplitterOutPin, AudioDecoderInPin)))
        {
            Quit("Unable to connect audio splitter and decoder pins."); return;
        }
    }

    {
        IPin* AudioDecoderOutPin = NULL;
        if (FAILED(State.AudioDecoder->FindPin(L"Out", &AudioDecoderOutPin)))
        {
            Quit("Unable to find audio decoder output pin."); return;
        }

        IPin* AudioRendererInPin = NULL;
        if (FAILED(State.AudioRenderer->FindPin(L"Audio Input pin (rendered)", &AudioRendererInPin)))
        {
            Quit("Unable to find audio renderer input pin."); return;
        }

        if (FAILED(State.GraphBuilder->Connect(AudioDecoderOutPin, AudioRendererInPin)))
        {
            Quit("Unable to connect audio decoder and renderer pins."); return;
        }
    }

}

extern "C" u32 PlayVideo(const char* videoFileName, const char* audioFileName, const HWND hwnd)
{
    State.HWND = hwnd;

    StringFormat(State.Buffer.VideoFileName, "%s.mpg", videoFileName);

    OFSTRUCT ofs = { 0 };
    if (OpenFile(State.Buffer.VideoFileName, &ofs, OF_EXIST) == HFILE_ERROR)
    {
        return FALSE;
    }

    if (FAILED(CoInitialize(NULL)))
    {
        Quit("Unable to initialize COM."); return FALSE;
    }

    if (FAILED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&State.GraphBuilder)))
    {
        CoUninitialize();
        Quit("Unable to initialize Graph Builder."); return FALSE;
    }

    if (FAILED(State.GraphBuilder->QueryInterface(IID_IMediaControl, (void**)&State.MediaControl)))
    {
        CoUninitialize();
        Quit("Unable to initialize Media Control."); return FALSE;
    }

    if (FAILED(State.GraphBuilder->QueryInterface(IID_IVideoWindow, (void**)&State.VideoWindow)))
    {
        CoUninitialize();
        Quit("Unable to initialize Video Window."); return FALSE;
    }

    if (FAILED(State.GraphBuilder->QueryInterface(IID_IMediaEventEx, (void**)&State.MediaEvent)))
    {
        CoUninitialize();
        Quit("Unable to initialize Media Wvent."); return FALSE;
    }

    StringToWideString(State.Buffer.VideoWideFileName, State.Buffer.VideoFileName);

    if (audioFileName == NULL)
    {
        if (FAILED(State.GraphBuilder->RenderFile(State.Buffer.VideoWideFileName, NULL)))
        {
            CoUninitialize();
            Quit("Unable to render file %s.", State.Buffer.VideoFileName); return FALSE;
        }
    }
    else
    {
        StringToWideString(State.Buffer.AudioWideFileName, audioFileName);

        Initialize();
        Configure();

        ShowWindow(State.HWND, SW_SHOWMAXIMIZED);
    }

    if (FAILED(State.VideoWindow->put_MessageDrain((OAHWND)State.HWND)))
    {
        CoUninitialize();
        Quit("Unable to set message drain."); return FALSE;
    }

    const auto width = GetSystemMetrics(SM_CXSCREEN);
    const auto height = GetSystemMetrics(SM_CYSCREEN);

    State.VideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
    State.VideoWindow->put_Left(0);
    State.VideoWindow->put_Top(0);
    State.VideoWindow->put_Width(width);
    State.VideoWindow->put_Height(height);
    State.VideoWindow->put_Owner((OAHWND)State.HWND);

    ShowCursor(FALSE);

    State.MediaControl->Pause();
    State.MediaControl->Run();

    State.IsQuit = FALSE;

    while (!State.IsQuit)
    {
        long eventCode = 0;
        LONG_PTR a = NULL;
        LONG_PTR b = NULL;

        if (State.MediaEvent->GetEvent(&eventCode, &a, &b, 0) == E_ABORT)
        {
            Sleep(100);
        }
        else
        {
            if (FAILED(State.MediaEvent->FreeEventParams(eventCode, a, b)))
            {
                Quit("Unable to free event parameters."); return FALSE;
            }

            if (eventCode == EC_COMPLETE)
            {
                State.IsQuit = TRUE;
            }

            Sleep(100);
        }

        MSG msg = { 0 };
        auto exists = PeekMessageA(&msg, State.HWND, 0, 0, PM_REMOVE);

        while (exists)
        {
            switch (msg.message)
            {
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            {
                State.IsQuit = TRUE;
                break;
            }
            case WM_MBUTTONUP:
            {
                break;
            }
            default:
            {
                if (msg.message == WM_LBUTTONDOWN || msg.message == WM_KEYFIRST
                    || msg.message == WM_CHAR || msg.message == WM_SYSKEYDOWN)
                {
                    State.IsQuit = TRUE;
                    break;
                }
            }
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);

            exists = PeekMessageA(&msg, State.HWND, 0, 0, PM_REMOVE);
        }

        if (State.IsQuit)
        {
            State.MediaControl->Stop();

            if (State.GraphBuilder != NULL)
            {
                State.GraphBuilder->Release();
                State.GraphBuilder = NULL;
            }

            if (State.MediaControl != NULL)
            {
                State.MediaControl->Release();
                State.MediaControl = NULL;
            }

            if (State.VideoWindow != NULL)
            {
                State.VideoWindow->Release();
                State.VideoWindow = NULL;
            }

            if (State.MediaEvent != NULL)
            {
                State.MediaEvent->Release();
                State.MediaEvent = NULL;
            }

            CoUninitialize();

            State.HWND = NULL;
            State.IsQuit = FALSE;
            return TRUE;
        }
    }

    return TRUE;
}

extern "C" void QuitVideo()
{
    State.IsQuit = TRUE;
}