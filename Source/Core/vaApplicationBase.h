///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Core/vaEvent.h"

#include "Core/vaUI.h"

#include "Rendering/vaRendering.h"

namespace Vanilla
{
    class vaDebugCanvas2D;
    class vaDebugCanvas3D;
    class vaRenderDevice;
    class vaXMLSerializer;
    class vaApplicationBase;

    // just keeping events separate for clarity - no other reason
    class vaApplicationEvents
    {
    public:
        vaEvent<void(void)>                 Event_Started;
        vaEvent<void(void)>                 Event_BeforeStopped;
        vaEvent<void(void)>                 Event_Stopped;
        vaEvent<void(void)>                 Event_MouseCaptureChanged;

        // Arguments: float deltaTime
        // Same as Event_TickEx during Running
        vaEvent<void( float )>              Event_Tick;             // 

        vaEvent<void( vaXMLSerializer & )>  Event_SerializeSettings;

    };

    // This is an argument to the TickEx callback and describes 3 potential states during which it can get called: initialization, tick-ing, shutting down
    enum class vaApplicationState
    {
        Initializing,                   // happens once, before Event_SerializeSettings, and this is where swapchain has not been created yet so do not do any render target sizes, but you can add a handler for Event_SerializeSettings
        Running,                        // happens during normal runtime
        ShuttingDown                    // happens once, just before the application shuts down
    };

    typedef std::function<void( vaRenderDevice &, vaApplicationBase &, float, vaApplicationState )>   vaApplicationLoopFunction;

    class vaApplicationBase : public vaApplicationEvents, protected vaUIPanel, public vaSingletonBase<vaApplicationBase>
    {
    public:
        struct Settings
        {
            wstring             WindowTitle                 = L"Unnamed";
            bool                WindowTitleAppendBasicInfo  = true;

            wstring             CmdLine;                    // please set application command line here

            int                 StartScreenPosX             = -1;
            int                 StartScreenPosY             = -1;
            int                 StartScreenWidth            = 1920;
            int                 StartScreenHeight           = 1080;
            vaFullscreenState   StartFullscreenState        = vaFullscreenState::Windowed;

            bool                Vsync                       = false;
            int                 FramerateLimit              = 0;

            Settings( ) = default;
            Settings( const wstring & windowTitle = L"Unnamed", const wstring & cmdLine = L"" ) : WindowTitle(windowTitle), CmdLine( cmdLine ) { }
        };

    protected:
        Settings                            m_settings;
        vector<pair<string, string>>        m_enumeratedAPIsAdapters;

        bool                                m_initialized;

        std::shared_ptr<vaRenderDevice>     m_renderDevice;

        vaSystemTimer                       m_mainTimer;
        float                               m_lastDeltaTime                     = 0.0f;
        //
        vaVector2i                          m_currentWindowClientSize;
        vaVector2i                          m_currentWindowPosition;
        vaVector2i                          m_lastNonFullscreenWindowClientSize;
        //
        static const int                    c_framerateHistoryCount             = 128;
        float                               m_frametimeHistory[c_framerateHistoryCount];
        int                                 m_frametimeHistoryLast;
        float                               m_avgFramerate;
        float                               m_avgFrametime;
        float                               m_accumulatedDeltaFrameTime;
        //
        bool                                m_running;
        bool                                m_shouldQuit;

        bool                                m_hasFocus;

        int64                               m_tickNumber                        = 0;

        bool                                m_blockInput;

        vaCameraBase                        m_uiCamera;
        int64                               m_uiCameraUpdateTickNumber          = -1;                       // just for testing

        // switch/value commandline pairs
        std::vector< std::pair<wstring, wstring> > m_cmdLineParams;

        wstring                             m_basicFrameInfo;

        // used for delayed switch to fullscreen or window size change
        vaVector2i                          m_setWindowSizeNextFrame;                                       // only works if not fullscreen 
        vaFullscreenState                   m_setFullscreenStateNextFrame       = vaFullscreenState::Unknown;   // next state

        vaFullscreenState                   m_currentFullscreenState            = vaFullscreenState::Unknown;   // this track current/last state and is valid after window is destroyed too (to enable serialization)

        const float                         m_windowTitleInfoUpdateFrequency    = 0.1f;                     // at least every 100ms - should be enough
        float                               m_windowTitleInfoTimeFromLastUpdate = 0;

        shared_ptr<void>                    m_aliveToken                        = std::make_shared<int>(42);
        vaApplicationLoopFunction const     m_tickExCallback;

        float                               m_uiScaling                         = 1.0f;                     // should be 1.0 at 1080p (1920x1080) and up/down accordingly

    protected:
        vaApplicationBase( const Settings & settings, const std::shared_ptr<vaRenderDevice> & renderDevice, const vaApplicationLoopFunction & callback );
        virtual ~vaApplicationBase( );

    protected:
        void                                UpdateFramerateStats( float deltaTime );
        virtual void                        Initialize( );
        virtual void                        Deinitialize( );
        //
        virtual void                        UIPanelTick( vaApplicationBase & application ) override;
        virtual void                        UIPanelTickAlways( vaApplicationBase & application ) override;
        //
    public:
        // run the main loop!
        virtual void                        Run( )                                  = 0;
        // quit the app (will finish current frame)
        void                                Quit( );
        // call this to invoke all the vaUI ImGui windows and panels stuff; if there's any 3D UI, provide the camera too
        void                                TickUI( const vaCameraBase & camera = vaCameraBase() );
        // call this at the end of your render frame to draw the ImGui as well as the stuff pooled into Canvas2D/Canvas3D. 
        // (at the moment this will only draw into the current backbuffer, but you can/should provide depth for any 3D, depth-aware UI)
        void                                DrawUI( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & depthBuffer );
        //
        const vaCameraBase &                GetUICamera( )                          { assert( m_uiCameraUpdateTickNumber != 1 ); return m_uiCamera; }
        //
    public:
        const vaSystemTimer &               GetMainTimer( ) const                   { return m_mainTimer; }
        double                              GetTimeFromStart( ) const               { return m_mainTimer.GetTimeFromStart(); }
        //        //
        float                               GetAvgFramerate( ) const                { return m_avgFramerate; }
        float                               GetAvgFrametime( ) const                { return m_avgFrametime; }
        //
        int64                               GetTickNumber( ) const                  { return m_tickNumber; }
        //
        virtual void                        CaptureMouse( )                         = 0;
        virtual void                        ReleaseMouse( )                         = 0;
        virtual bool                        IsMouseCaptured( ) const;
        //
        const Settings &                    GetSettings( ) const                    { return m_settings; }
        //
        vaFullscreenState                   GetFullscreenState( ) const             { return ( m_setFullscreenStateNextFrame != vaFullscreenState::Unknown ) ? ( m_setFullscreenStateNextFrame ) : ( m_currentFullscreenState ); }
        void                                SetFullscreenState( vaFullscreenState state ) { m_setFullscreenStateNextFrame = state; }
        bool                                IsFullscreen( ) const                   { return GetFullscreenState() != vaFullscreenState::Windowed; }

        bool                                HasFocus( ) const                       { return m_hasFocus; }
        
        bool                                IsInputBlocked( ) const                 { return m_blockInput; }
        void                                SetBlockInput( bool blockInput )        { m_blockInput = blockInput; }

        const vector<pair<wstring,wstring>> &      GetCommandLineParameters( ) const       { return m_cmdLineParams; }

        const wstring &                     GetBasicFrameInfoText( )                { return m_basicFrameInfo; }

        vaRenderDevice &                    GetRenderDevice( )                      { return *m_renderDevice; }

        void                                SetWindowTitle( wstring & windowTitle, bool appendBasicInfo )    { m_settings.WindowTitle = windowTitle; m_settings.WindowTitleAppendBasicInfo = appendBasicInfo; }

        bool                                GetVsync( ) const                       { return m_settings.Vsync; }
        void                                SetVsync( bool vsync )                  { m_settings.Vsync = vsync; }

        int                                 GetFramerateLimit( ) const              { return m_settings.FramerateLimit; }
        void                                SetFramerateLimit( int fpsLimit )       { m_settings.FramerateLimit = fpsLimit; }

        virtual vaVector2i                  GetWindowPosition( ) const = 0;
        virtual void                        SetWindowPosition( const vaVector2i & position ) = 0;
        virtual vaVector2i                  GetWindowClientAreaSize( ) const = 0;
        virtual void                        SetWindowClientAreaSize( const vaVector2i & clientSize ) = 0;

        virtual void                        OnGotFocus( );
        virtual void                        OnLostFocus( );

        virtual void                        Tick( float deltaTime );

        virtual wstring                     GetSettingsFileName()                   { return vaCore::GetExecutableDirectory( ) + L"ApplicationSettings.xml"; }
        virtual void                        NamedSerializeSettings( vaXMLSerializer & serializer );

        vaDebugCanvas2D &                   GetCanvas2D( );
        vaDebugCanvas3D &                   GetCanvas3D( );

        float                               GetUIScaling( ) const                   { return m_uiScaling; }

    public:
        virtual void                        UIPanelHandler( vaApplicationBase & ) ;

        // if blocking the main thread to wait for the same thread that just started a messagebox and expects input, you have to call this in the waiting loop - messy, not to be used except for error reporting / debugging
        virtual bool                        MessageLoopTick( ) = 0;


    private:
        //
        bool                                UpdateUserWindowChanges( );

    public:
        static wstring                      GetDefaultGraphicsAPIAdapterInfoFileName()  { return vaCore::GetExecutableDirectory( ) + L"APIAdapter"; }
        static void                         SaveDefaultGraphicsAPIAdapter( pair<string, string> apiAdapter );
        static pair<string, string>         LoadDefaultGraphicsAPIAdapter( );
    };
}