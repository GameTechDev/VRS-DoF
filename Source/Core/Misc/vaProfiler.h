///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
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

// Perhaps consider using Tracy (https://bitbucket.org/wolfpld/tracy/src/master/) instead?
// Or https://github.com/bombomby/optick - also free.

#include "Core/vaCoreIncludes.h"

namespace Vanilla
{
    class vaRenderDeviceContext;

// enable this to use any temporary string for naming traces (name gets copied) - slower but works with custom formatting and etc.
#define VA_TRACER_ALLOW_NON_CONST_NAMES

    class vaTracerView;

    // multithreaded timeline-based begin<->end tracing with built-in json output for chrome://tracing!
    // for details and extension ideas, see https://aras-p.info/blog/2017/01/23/Chrome-Tracing-as-Profiler-Frontend/ and 
    // https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU and
    // https://www.gamasutra.com/view/news/176420/Indepth_Using_Chrometracing_to_view_your_inline_profiling_data.php
    class vaTracer
    {
    public:
        struct Entry
        {
            double                                              Beginning;
            double                                              End;
#ifdef VA_TRACER_ALLOW_NON_CONST_NAMES
            string                                              Name;           // name seen from profilers
#else
            const char *                                        Name;
#endif
            int                                                 Depth;          // depth used to determine inner/outer if Beginning/End-s are same

#ifdef VA_TRACER_ALLOW_NON_CONST_NAMES
            Entry( const string & name, int depth, const double & beginning ) : Name(name), Depth(depth), Beginning(beginning), End(beginning) { }
#endif
            Entry( const char * name, int depth, const double & beginning ) : Name(name), Depth(depth), Beginning(beginning), End(beginning) { }

            Entry( ) { }
        };

        struct ThreadContext
        {
            string                                              Name;
            std::thread::id                                     ThreadID;               // or uninitialized for 'virtual' contexts (such as used for GPU tracing)
            bool                                                AutomaticFrameIncrement;

            std::mutex                                          TimelineMutex;
            std::deque<Entry>                                   Timeline;               // secured with TimelineMutex!!!

            int                                                 SortOrderCounter = 0;

            std::vector<Entry>                                  LocalTimeline;
            std::vector<int>                                    CurrentOpenStack;       // stack of LocalTimeline indices
            double                                              NextDefragTime   = 0;

            std::weak_ptr<vaTracerView>                         AttachedViewer;         // secured with TimelineMutex!!!

            ThreadContext( const char * name, const std::thread::id & id = std::thread::id(), bool automaticFrameIncrement = true );
            ~ThreadContext( );

            // inline void                                         OnEvent( const string & name )  { name; assert( false ); }

            inline void                                         OnBegin( const string & name )
            {
#ifdef VA_TRACER_ALLOW_NON_CONST_NAMES
                OnBegin( name.c_str() );
#else
                name;
                OnBegin( "non_const_names_not_enabled" );
#endif
            }

            inline void                                         OnBegin( const char * name )    
            { 
                auto now = vaCore::TimeFromAppStart();
                LocalTimeline.emplace_back( name, (int)CurrentOpenStack.size( ), now );
                CurrentOpenStack.push_back( (int)LocalTimeline.size( ) - 1 );
            }

#ifdef _DEBUG
            inline void                                         OnEnd( const string & verifyName );
#else
            inline void                                         OnEnd( );
#endif

            inline void                                         BatchAddFrame( Entry * entries, int count );

            inline void                                         Capture( std::deque<Entry> & outEntries, bool move = true )
            {
                std::lock_guard<std::mutex> lock( TimelineMutex );
                outEntries;
                move;
                if( move )
                    outEntries = std::move( Timeline );
                else
                    outEntries = Timeline;
            }
            inline void                                         CaptureLast( std::vector<Entry>& outEntries, const double & oldestAge )
            {
                std::lock_guard<std::mutex> lock( TimelineMutex );
                for( auto it = Timeline.rbegin(); it != Timeline.rend(); it++ )
                {
                    outEntries.emplace_back( *it );
                    if( it->Depth == 0 && it->End < oldestAge ) // stop only at depth == 0 or otherwise it will be ugly to parse
                        break;
                }
            }
        };

    private:
        friend class vaTracerView;

        static std::mutex                                       s_globalMutex;
        static //std::map< std::thread::id, std::weak_ptr<ThreadContext> >
               std::vector< std::weak_ptr<ThreadContext> >
                                                                s_threadContexts;
        static weak_ptr<ThreadContext>                          s_mainThreadContext;
        static constexpr double                                 c_maxCaptureDuration  = 5.0; // 5 seconds
//
//        static thread_local shared_ptr<Thread>                  s_threads;

    private:
        inline static shared_ptr<ThreadContext> &               LocalThreadContextSharedPtr( )
        {
            static thread_local shared_ptr<ThreadContext> localThreadContext = nullptr;

            return localThreadContext;
        }

    public:
        inline static ThreadContext *                           LocalThreadContext( )
        {
            shared_ptr<ThreadContext> & localThreadContext = LocalThreadContextSharedPtr();
            if( localThreadContext == nullptr )
            {
                std::lock_guard<std::mutex> lock( s_globalMutex );
                if( vaThreading::ThreadLocal().MainThreadSynced )
                { 
                    assert( !vaThreading::ThreadLocal().MainThread );
                    localThreadContext = s_mainThreadContext.lock();
                    assert( localThreadContext != nullptr );
                }
                else
                {
                    localThreadContext = std::make_shared<ThreadContext>( vaThreading::GetThreadName(), std::this_thread::get_id() );
                    // s_threadContexts.emplace( std::this_thread::get_id(), localThreadContext );
                    s_threadContexts.push_back( localThreadContext );
                    if( vaThreading::ThreadLocal().MainThread )
                    {
                        assert( !vaThreading::ThreadLocal().MainThreadSynced );
                        s_mainThreadContext = localThreadContext;
                    }
                }
            }
            return localThreadContext.get();
        }

        // caller is responsible for keeping it alive! we only keep a weak reference and it gets deleted if not there
        inline static shared_ptr<ThreadContext>                 CreateVirtualThreadContext( const char* name )
        {
            std::lock_guard<std::mutex> lock( s_globalMutex );
            shared_ptr<ThreadContext> retContext = std::make_shared<ThreadContext>( name, std::thread::id(), false );
            // s_threadContexts.emplace( std::this_thread::get_id(), localThreadContext );
            s_threadContexts.push_back( retContext );
            return retContext;
        }

        static void                                             DumpChromeTracingReportToFile( double duration = c_maxCaptureDuration, bool reset = true );
        static string                                           CreateChromeTracingReport( double duration = c_maxCaptureDuration, bool reset = true );
        static void                                             ListAllThreadNames( vector<string> & outNames );
        //static void                                             UpdateToView( vaTracerView & outView, float historyDuration );

        static constexpr float                                  c_UI_ProfilingUpdateFrequency   = 1.0f;
        static float                                            m_UI_ProfilingTimeToNextUpdate;
        static vector<string>                                   m_UI_ProfilingThreadNames;
        static int                                              m_UI_ProfilingSelectedThreadIndex;
        
        static shared_ptr<vaTracerView>                         m_UI_TracerViewActiveCollect;       // this one collects the data while the other one 
        static shared_ptr<vaTracerView>                         m_UI_TracerViewDisplay;

        static bool                                             m_UI_TracerViewingEnabled;

    public:
        static bool                                             IsTracerViewingUIEnabled( )                 { return m_UI_TracerViewingEnabled;     }
        static void                                             SetTracerViewingUIEnabled( bool enable );

        static void                                             TickImGui( class vaApplicationBase & application, float deltaTime );

        // // don't hold this pointer or use it from non-main thread - none of these are supported!
        // static shared_ptr<vaTracerView const>                   GetViewableTracerView( )            { assert( vaThreading::IsMainThread() ); return m_UI_TracerViewDisplay; };


    private:
        friend vaCore;
        static void                                             Cleanup( bool soft );
    };

    // A look into traces on a specific thread captured by vaTracer; 
    // will provide Imgui UI too and some tools for it.
    class vaTracerView : public std::enable_shared_from_this<vaTracerView>
    {
    public:
        struct Node
        {
            string              Name;
            int                 SortOrder       = 0;      // order in additions; just so the display is somewhat consistent

            // these times are relative to vaTracerReport::Beginning, converted to milliseconds
            // they represent total recorded during the vaTracerReport::end-beginning time span (excluding parts of the sections outside the time span)
            double              TimeTotal           = 0.0;
            double              TimeTotalAvgPerInst = 0.0;
            double              TimeTotalAvgPerFrame= 0.0;
            double              TimeTotalMax        = 0.0;   
            double              TimeTotalMin        = 0.0;   
            double              TimeSelfAvgPerFrame = 0.0;
            int                 Instances           = 0;        // how many times was recorded during the vaTracerReport::end-beginning time span

            int                 RecursionDepth  = 0;        // RootNode is 0

            vector<Node*>       ChildNodes;

            // UI parameters
            bool                Opened          = true;
            bool                Selected        = false;
            int                 LastSeenAge     = 0;        // how many last Update-s in a row it had 0 instances (will be removed after n)
            
            static const int    LastSeenAgeToKeepAlive      = 2;

        private:
            friend vaTracerView;
            void Reset( bool fullReset )
            {
                if( fullReset )
                {
                    assert( ChildNodes.size() == 0 );
                    Name            = "";
                    Opened          = true;
                    Selected        = false;
                    LastSeenAge     = 0;
                }
                TimeTotal               = 0.0;
                TimeTotalAvgPerInst     = 0.0;
                TimeTotalAvgPerFrame    = 0.0;
                TimeTotalMin            = -0.0;
                TimeTotalMax            = -0.0;
                TimeSelfAvgPerFrame     = 0.0;
                Instances               = 0;
                SortOrder               = 0;
            }

            void ReleaseRecursive( vaTracerView & view )   
            { 
                for( int i = 0; i < ChildNodes.size(); i++ )
                    ChildNodes[i]->ReleaseRecursive( view );
                ChildNodes.clear();
                
                view.ReleaseNode( this );
            }

            void PreUpdateRecursive( )
            {
                for( int i = 0; i < ChildNodes.size( ); i++ )
                    ChildNodes[i]->PreUpdateRecursive( );

                Reset( false );
            }

            void SyncUIRecursive( vaTracerView & view, const Node* srcNode )
            {
                Opened      = srcNode->Opened;
                Selected    = srcNode->Selected;
                LastSeenAge = std::min(LastSeenAge, srcNode->LastSeenAge);

                for( int j = 0; j < srcNode->ChildNodes.size( ); j++ )
                {
                    bool found = false;
                    for( int i = 0; i < ChildNodes.size( ); i++ )
                    {
                        if( ChildNodes[i]->Name == srcNode->ChildNodes[j]->Name )
                        {
                            ChildNodes[i]->SyncUIRecursive( view, srcNode->ChildNodes[j] );
                            found = true;
                            break;
                        }
                    }
                    if( !found )
                    {
                        vaTracerView::Node* newNode;
                        ChildNodes.push_back( newNode = view.AllocateNode( ) );
                        newNode->Reset( true );
                        newNode->Name = srcNode->ChildNodes[j]->Name;
                        newNode->RecursionDepth = srcNode->ChildNodes[j]->RecursionDepth;
                        newNode->SyncUIRecursive( view, srcNode->ChildNodes[j] );
                    }
                }
            }

            void PostUpdateRecursive( vaTracerView & view )
            {
                LastSeenAge++;
                double childrenTimeTotalAvgPerFrame = 0.0;
                for( int i = (int)ChildNodes.size()-1; i >= 0 ; i-- )
                {
                    ChildNodes[i]->PostUpdateRecursive( view );
                    childrenTimeTotalAvgPerFrame += ChildNodes[i]->TimeTotalAvgPerFrame;

                    if( ChildNodes[i]->LastSeenAge > Node::LastSeenAgeToKeepAlive )
                    {
                        ChildNodes[i]->ReleaseRecursive( view );
                        ChildNodes[i] = nullptr;
                        if( ChildNodes.size( ) - 1 > i )
                            ChildNodes[i] = ChildNodes[ChildNodes.size( ) - 1];
                        ChildNodes.pop_back( );
                    }

                }

                TimeTotalAvgPerInst     = TimeTotal / (double)Instances;
                TimeTotalAvgPerFrame    = TimeTotal / (double)view.m_frameCountWhileConnected;
                TimeSelfAvgPerFrame     = TimeTotalAvgPerFrame - childrenTimeTotalAvgPerFrame;

                std::sort( ChildNodes.begin( ), ChildNodes.end( ), [ ]( const Node* a, const Node* b ) { return a->SortOrder < b->SortOrder; } ); // { return a->Name < b->Name; } );
            }

            const Node * FindRecursive( const string & name ) const
            {
                if( name == this->Name )
                    return this;
                else
                {
                    for( int i = 0; i < ChildNodes.size(); i++ )
                    {
                        const Node * retVal = ChildNodes[i]->FindRecursive( name );
                        if( retVal != nullptr )
                            return retVal;
                    }
                }
                return nullptr;
            }

        };
    private:
        string                  m_connectionName;
        weak_ptr<vaTracer::ThreadContext>
                                m_connectedThreadContext;
        mutable std::mutex      m_viewMutex;

        bool                    m_nameChanged;

        vector<Node*>           m_rootNodes;

        vector<vaTracer::Entry> CapturedTimeline;

        vector<Node*>           UnusedNodePool;

        int                     m_frameCountWhileConnected;
        int                     m_frameSortCounter;
        double                  m_lastConnectedTime;
        double                  m_connectionTimeoutTime;

//        vaGUID                  m_lastConnectionGUID    = vaCore::GUIDNull();

        void                    Reset( )                                            
        { 
            std::lock_guard<std::mutex> lock(m_viewMutex);
            m_connectedThreadContext.reset();
            for( int i = 0; i < m_rootNodes.size(); i++ ) 
                m_rootNodes[i]->ReleaseRecursive( *this ); 
            m_rootNodes.clear(); 
            m_connectionName = ""; 
            /*Beginning = End = std::chrono::time_point<std::chrono::steady_clock>();*/ 
            for( int i = 0; i < UnusedNodePool.size( ); i++ )
                delete UnusedNodePool[i];
            UnusedNodePool.clear();
            m_frameCountWhileConnected  = 0;
            m_frameSortCounter          = 0;
            m_lastConnectedTime         = 0;
            m_connectionTimeoutTime     = 0;
            m_nameChanged               = true;
        }

    protected:
        friend Node;
        friend vaTracer::ThreadContext;
        Node *              AllocateNode( )                                         
        { 
            if( UnusedNodePool.size() == 0 ) 
                return new Node; 
            Node * ret = UnusedNodePool.back(); 
            UnusedNodePool.pop_back(); 
            return ret; 
        }
        void                ReleaseNode( Node * node )                              
        { 
            assert( node->ChildNodes.size() == 0 ); 
            if( UnusedNodePool.size() < 10000 )
                UnusedNodePool.push_back( node );
            else
                delete node;
        }
        void                PreUpdateRecursive( );
        void                PostUpdateRecursive( );
        void                UpdateCallback( vaTracer::Entry * timelineChunk, int timelineChunkCount, bool incrementFrameCounter = false );

        friend class vaTracer;
        void                TickFrame( float deltaTime );

    public:
        vaTracerView( )     { Reset(); }
        ~vaTracerView( )    { Reset(); }

        const string &      GetLastConnectionName( ) const { return m_connectionName; }
//        const vaGUID &      GetLastConnectionGUID( ) const { return m_lastConnectionGUID; }

        void                ConnectToThreadContext( const string & name, float connectionTimeout );       // basic wildcards supported so "!!GPU*" will work with the first GPU context
        void                Disconnect( shared_ptr<vaTracerView> syncReportUI = nullptr );
        bool                IsConnected( ) const                            { assert( vaThreading::IsMainThread( ) ); std::lock_guard<std::mutex> lock( m_viewMutex ); return !m_connectedThreadContext.expired(); }

        void                TickImGuiRecursive( Node * node );
        void                TickImGui( );

        // don't hold this pointer or use it from non-main thread - none of these are supported!
        const Node *        FindNodeRecursive( const string & name ) const;
    };

#ifdef _DEBUG
    inline void vaTracer::ThreadContext::OnEnd( const string & verifyName )
#else
    inline void vaTracer::ThreadContext::OnEnd( )
#endif
    {
        double now = vaCore::TimeFromAppStart( );
        assert( CurrentOpenStack.size( ) > 0 );
        if( CurrentOpenStack.size( ) == 0 )
            return;

#ifdef _DEBUG
        // if this triggers, you have overlapping scopes - shouldn't happen but it did so fix it please :)
        assert( verifyName == LocalTimeline[CurrentOpenStack.back( )].Name );
#endif
        LocalTimeline[CurrentOpenStack.back( )].End = now;
        CurrentOpenStack.pop_back( );

        if( CurrentOpenStack.size( ) == 0 && ( LocalTimeline.size( ) > 20 || NextDefragTime > now ) )
        {
            std::lock_guard<std::mutex> lock( TimelineMutex );

            // if there's a viewer attached
            auto attachedViewer = AttachedViewer.lock( );
            if( attachedViewer != nullptr ) // if attached viewer callback returns false, we're no longer connected
                attachedViewer->UpdateCallback( LocalTimeline.data(), (int)LocalTimeline.size() );

            const int arrsize = (int)LocalTimeline.size( );
            for( int i = 0; i < arrsize; i++ )
                Timeline.emplace_back( std::move(LocalTimeline[i]) );

            // done with this, clear!
            LocalTimeline.clear( );

            // remove older (do it here because this way there is no global locking and the cost is spread across all threads and is proportional to use)
            if( NextDefragTime > now )
            {
                NextDefragTime = now + 0.05f;   // 20 times per second sounds reasonable?
                auto oldest = now - c_maxCaptureDuration;
                while( Timeline.size( ) > 0 && Timeline.begin( )->Beginning < oldest )
                    Timeline.pop_front( );
            }
        }
    }

    inline void vaTracer::ThreadContext::BatchAddFrame( Entry* entries, int count )
    {
        assert( CurrentOpenStack.size( ) == 0 );
        if( CurrentOpenStack.size( ) != 0 )
            return;

        double now = vaCore::TimeFromAppStart( );

        std::lock_guard<std::mutex> lock( TimelineMutex );

        // if there's a viewer attached
        auto attachedViewer = AttachedViewer.lock( );
        if( attachedViewer != nullptr ) // if attached viewer callback returns false, we're no longer connected
        {
            assert( AutomaticFrameIncrement == false );
            attachedViewer->UpdateCallback( entries, count, true );
        }

        for( int i = 0; i < count; i++ )
            Timeline.emplace_back( std::move(entries[i]) );

        // cleanup always
        {
            auto oldest = now - c_maxCaptureDuration;
            while( Timeline.size( ) > 0 && Timeline.begin( )->Beginning < oldest )
                Timeline.pop_front( );
        }
    }

#define VA_SCOPE_TRACE_ENABLED

#ifdef VA_SCOPE_TRACE_ENABLED
    struct vaScopeTrace
    {
        vaRenderDeviceContext * const       m_renderDeviceContext   = nullptr;
        int                                 m_GPUTraceHandle        = -1;

#ifdef _DEBUG
        string const                        m_name;
#endif

        vaScopeTrace( const char * name ) 
#ifdef _DEBUG
            : m_name( name )
#endif
        { 
            vaTracer::LocalThreadContext( )->OnBegin( name ); 
            BeginCPUTrace( name );
        }
        vaScopeTrace( const char * name, vaRenderDeviceContext * renderDeviceContext ) 
            : 
#ifdef _DEBUG
            m_name( name ), 
#endif
            m_renderDeviceContext(renderDeviceContext)  
        { 
            vaTracer::LocalThreadContext( )->OnBegin( name ); 
            BeginGPUTrace( name );
        }
        ~vaScopeTrace( )                    
        { 
            if(m_renderDeviceContext != nullptr) 
                EndGPUTrace(); 
            else
                EndCPUTrace();
#ifdef _DEBUG
            vaTracer::LocalThreadContext( )->OnEnd( m_name ); 
#else
            vaTracer::LocalThreadContext( )->OnEnd( ); 
#endif
        }

    private:
        void                                BeginGPUTrace( const char * name );
        void                                EndGPUTrace();
        void                                BeginCPUTrace( const char * name );
        void                                EndCPUTrace();
    };
    #define VA_TRACE_CPU_SCOPE( name )                                          vaScopeTrace scope_##name( #name );
    #define VA_TRACE_CPU_SCOPE_CUSTOMNAME( nameVar, customName )                vaScopeTrace scope_##name( customName );
    #define VA_TRACE_CPUGPU_SCOPE( name, apiContext )                           vaScopeTrace scope_##name( #name, &apiContext );
    #define VA_TRACE_CPUGPU_SCOPE_CUSTOMNAME( nameVar, customName )             vaScopeTrace scope_##name( customName, &apiContext );
    #define VA_TRACE_MAKE_LAST_SELECTED( )                                      //    do { vaProfiler::GetInstance().MakeLastScopeSelected( ); } while( false )
#else
    #define VA_TRACE_CPU_SCOPE( name )                             
    #define VA_TRACE_CPU_SCOPE_CUSTOMNAME( nameVar, customName )   
    #define VA_TRACE_CPUGPU_SCOPE( name, apiContext )              
    #define VA_TRACE_CPUGPU_SCOPE_CUSTOMNAME( nameVar, customName )
    #define VA_TRACE_MAKE_LAST_SELECTED( )                         
#endif

}
