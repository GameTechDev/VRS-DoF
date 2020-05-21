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

#include "Rendering/vaGPUTimer.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include <vector>

namespace Vanilla
{
    class vaGPUContextTracerDX11 : public vaGPUContextTracer
    {
    protected:
        static const int                            c_bufferCount = vaRenderDevice::c_BackbufferCount + 1;

        vector<ComPtr<ID3D11Query>>                 m_disjointQueriesPool;
        vector<ComPtr<ID3D11Query>>                 m_timerQueriesPool;

        struct TraceEntry
        {
            string                  Name;
            int                     Depth;

            ComPtr<ID3D11Query>     DisjointQ;
            ComPtr<ID3D11Query>     BeginQ;
            ComPtr<ID3D11Query>     EndQ;
        };

        TraceEntry                                  m_traceEntries[vaGPUContextTracer::c_maxTraceCount][c_bufferCount];
        double                                      m_traceBaseTime[c_bufferCount];

        //ComPtr<ID3D12QueryHeap>                     m_queryHeap;
        //shared_ptr<vaBufferDX12>                    m_queryReadbackBuffers[c_bufferCount];

        int                                         m_currentTraceIndex[c_bufferCount];

        int                                         m_currentBufferSet = 0;

        // double                                      m_CPUTimeAtSync = 0;
        // uint64                                      m_GPUTimestampAtSync = 0;
        // uint64                                      m_GPUTimestampFrequency = 0;

        int                                         m_recursionDepth = 0;

        //int                                         m_wholeFrameTraceHandle = -1;

        // some temps used while processing
        bool                                        m_dataOkArr[vaGPUContextTracer::c_maxTraceCount];
        double                                      m_timeBeginArr[vaGPUContextTracer::c_maxTraceCount];
        double                                      m_timeEndArr[vaGPUContextTracer::c_maxTraceCount];


    public:
        vaGPUContextTracerDX11( const vaRenderingModuleParams& params );
        virtual ~vaGPUContextTracerDX11( );

    private:
        ComPtr<ID3D11Query>                         AllocQuery( bool timerType );       // if not timer then it's disjoint
        void                                        ReleaseQuery( ComPtr<ID3D11Query> & query, bool timerType );     // if not timer then it's disjoint
        
    protected:
        virtual void                                BeginFrame( ) override;
        virtual void                                EndFrame( ) override;

    public:
        virtual int                                 Begin( const string& name ) override;
        virtual void                                End( int handle ) override;
    };
}
