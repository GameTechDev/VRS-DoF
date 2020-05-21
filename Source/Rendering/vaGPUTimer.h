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

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaRenderingIncludes.h"

#include <vector>

namespace Vanilla
{
    struct vaGPUContextTracerParams : vaRenderingModuleParams
    {
        vaRenderDeviceContext& RenderDeviceContext;
        vaGPUContextTracerParams( vaRenderDeviceContext& renderDeviceContext );
    };

    class vaGPUContextTracer : public vaRenderingModule
    {
    public:
        static const int                            c_maxTraceCount     = 4096;     // max traces per frame
        static constexpr const char *               c_threadNamePrefix  = "!!GPU";

    protected:
        vaRenderDeviceContext &                     m_renderContext;

        bool                                        m_active            = false;
        shared_ptr<vaTracer::ThreadContext>         m_threadContext     = nullptr;

    protected:

    protected:
        vaGPUContextTracer( const vaGPUContextTracerParams & params );
    public:
        virtual ~vaGPUContextTracer( ) { }

    public:
        virtual int                                 Begin( const string & name )            = 0;
        int                                         Begin( const char * name )              { return Begin( string(name) ); }
        virtual void                                End( int handle )                       = 0;

    protected:
        friend class vaRenderDeviceContext;
        virtual void                                BeginFrame( )                           = 0;
        virtual void                                EndFrame( )                             = 0;
    };
}
