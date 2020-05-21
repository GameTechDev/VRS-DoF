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

#include "Rendering/DirectX/vaGPUTimerDX11.h"

#include "Rendering/DirectX/vaRenderDeviceContextDX11.h"

#include <algorithm>

//#include <pix.h>

using namespace Vanilla;

vaGPUContextTracerDX11::vaGPUContextTracerDX11( const vaRenderingModuleParams& params ) : vaGPUContextTracer( vaSaferStaticCast< const vaGPUContextTracerParams&, const vaRenderingModuleParams&>( params ) )
{
    for( int i = 0; i < c_bufferCount; i++ )
        m_currentTraceIndex[i] = 0;
}

vaGPUContextTracerDX11::~vaGPUContextTracerDX11( )
{

}

ComPtr<ID3D11Query> vaGPUContextTracerDX11::AllocQuery( bool timerType )
{

    std::vector<ComPtr<ID3D11Query>> & container = (timerType)?(m_timerQueriesPool):(m_disjointQueriesPool);
    if( container.size( ) == 0 )
    {
        CD3D11_QUERY_DESC tqd( ( timerType ) ? ( D3D11_QUERY_TIMESTAMP ) : ( D3D11_QUERY_TIMESTAMP_DISJOINT ), 0 );
        ID3D11Query * ptr = nullptr;
        if( FAILED( m_renderDevice.SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice( )->CreateQuery( &tqd, &ptr ) ) )
        {
            assert( false );
            return nullptr;
        }
        ComPtr<ID3D11Query> ret = ptr;
        ptr->Release();
        return ret;
    }
    else
    {
        ComPtr<ID3D11Query> ret = container.back();
        container.pop_back();
        return ret;
    }
}

void vaGPUContextTracerDX11::ReleaseQuery( ComPtr<ID3D11Query> & query, bool timerType )
{
    if( timerType )
        m_timerQueriesPool.push_back( query );
    else
        m_disjointQueriesPool.push_back( query );
    query.Reset();
}


void vaGPUContextTracerDX11::BeginFrame( )
{
    // ID3D11Device * d3d11Device = m_renderDevice.SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice( );
    
    assert( m_recursionDepth == 0 );
    
    // no 'GetclockCalibration' on DX11 :(
    //if( m_GPUTimestampFrequency == 0 )
    //{
    //    uint64 CPUTimestampAtSync = 0;
    //    AsDX12( GetRenderDevice( ) ).GetCommandQueue( )->GetTimestampFrequency( &m_GPUTimestampFrequency );
    //    AsDX12( GetRenderDevice( ) ).GetCommandQueue( )->GetClockCalibration( &m_GPUTimestampAtSync, &CPUTimestampAtSync );
    //
    //    m_CPUTimeAtSync = ( CPUTimestampAtSync - vaCore::NativeAppStartTime( ) ) / double( vaCore::NativeTimerFrequency( ) );
    //}

    assert( GetRenderDevice( ).IsRenderThread( ) );
    assert( !m_active );
    m_currentTraceIndex[m_currentBufferSet] = 0;
    m_traceBaseTime[m_currentBufferSet] = vaCore::TimeFromAppStart();
    m_active = true;

    //m_wholeFrameTraceHandle = Begin( "WholeFrame" );
}

static bool GetQueryDataHelper( ID3D11DeviceContext* context, ID3D11Query* query, void* data, int dataSize )
{
    assert( query != nullptr );
    if( query == nullptr )
        return false;

    HRESULT hr = context->GetData( query, data, dataSize, D3D11_ASYNC_GETDATA_DONOTFLUSH );
    if( hr == S_OK )
        return true;
    else
    {
        //assert( false );
        return false;
    }
}


void vaGPUContextTracerDX11::EndFrame( )
{
    //ID3D11Device * d3d11Device = m_renderDevice.SafeCast<vaRenderDeviceDX11*>( )->GetPlatformDevice( );
    ID3D11DeviceContext * d3d11DeviceContext = m_renderContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext();
    
    assert( GetRenderDevice( ).IsRenderThread( ) );
    assert( m_active );
    //End( m_wholeFrameTraceHandle ); m_wholeFrameTraceHandle = -1;
    assert( m_recursionDepth == 0 );

    m_active = false;

    // // ok, we've done submitting all queries for this frame; now issue the copy-to-buffer command so we can read it in the future
    // AsDX12( m_renderContext ).GetCommandList( )->ResolveQueryData( m_queryHeap.Get( ), D3D12_QUERY_TYPE_TIMESTAMP, 0, m_currentTraceIndex[m_currentBufferSet] * 2, m_queryReadbackBuffers[m_currentBufferSet]->GetResource( ).Get( ), 0 );

    // ok, move to next
    m_currentBufferSet = ( m_currentBufferSet + 1 ) % c_bufferCount;

    // first get the old resolved data and update 
    if( m_currentTraceIndex[m_currentBufferSet] > 0 )   // only copy if there's anything to copy
    {
        const bool loopUntilDone = false;

        double baseFrameTime = m_traceBaseTime[m_currentBufferSet];

        const int entryCount = m_currentTraceIndex[m_currentBufferSet];
        
        bool * dataOkArr      = m_dataOkArr;
        double * timeBeginArr = m_timeBeginArr;
        double * timeEndArr   = m_timeEndArr;

        double minBeginTime = std::numeric_limits<double>::max();
        double prevBeginTime = 0.0;
        for( int i = 0; i < entryCount; i++ )
        {
            TraceEntry & platformEntry = m_traceEntries[i][m_currentBufferSet];

            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT      DisjointData;
            UINT64                                   BeginData;
            UINT64                                   EndData;
            bool & dataOk = dataOkArr[i];
            dataOk = true;
            dataOk &= GetQueryDataHelper( d3d11DeviceContext, platformEntry.DisjointQ.Get(), &DisjointData, sizeof( DisjointData ) );
            dataOk &= GetQueryDataHelper( d3d11DeviceContext, platformEntry.BeginQ.Get(), &BeginData, sizeof( BeginData ) );
            dataOk &= GetQueryDataHelper( d3d11DeviceContext, platformEntry.EndQ.Get(), &EndData, sizeof( EndData ) );

            ReleaseQuery( platformEntry.DisjointQ, false );
            ReleaseQuery( platformEntry.BeginQ, true );
            ReleaseQuery( platformEntry.EndQ, true );
            if( !dataOk )
                continue;

            dataOk &= !(( DisjointData.Disjoint ) || ( ( BeginData & 0xFFFFFFFF ) == 0xFFFFFFFF ) || ( ( EndData & 0xFFFFFFFF ) == 0xFFFFFFFF ));

            if( dataOk )
            {
                timeBeginArr[i] = BeginData / (double)DisjointData.Frequency;
                timeEndArr[i] = EndData / (double)DisjointData.Frequency;
                minBeginTime = std::min( minBeginTime, timeBeginArr[i] );
            }

            if(     (timeEndArr[i] < timeBeginArr[i])
                ||  (prevBeginTime > timeBeginArr[i]) )
                dataOk = false;
                
            if( !dataOk )
            {
                assert( false );
            }
            else
            {
                prevBeginTime = timeBeginArr[i];
            }
        }


        vaTracer::Entry outEntries[c_maxTraceCount];
        int outEntryCount = 0;
        prevBeginTime = baseFrameTime;
        for( int i = 0; i < entryCount; i++ )
        {
            if( !dataOkArr[i] )
                continue;
            TraceEntry& platformEntry = m_traceEntries[i][m_currentBufferSet];
            vaTracer::Entry& outEntry = outEntries[outEntryCount];
            outEntryCount++;
            outEntry.Name = platformEntry.Name;
            outEntry.Depth = platformEntry.Depth;
            outEntry.Beginning = baseFrameTime + (timeBeginArr[i] - minBeginTime);
            outEntry.End = baseFrameTime + ( timeEndArr[i] - minBeginTime );

            // clamp in case of math imprecision above
            if( outEntry.Beginning < prevBeginTime )
                outEntry.Beginning = prevBeginTime;
            if( outEntry.End < outEntry.Beginning )
                outEntry.End = outEntry.Beginning;
        }

        m_threadContext->BatchAddFrame( outEntries, outEntryCount );
    }
    m_currentTraceIndex[m_currentBufferSet] = 0; // reset, free to start collecting again
}

int vaGPUContextTracerDX11::Begin( const string& name )
{
    assert( m_recursionDepth >= 0 );
    assert( name != "" );

#if 1
    int currentIndex = m_currentTraceIndex[m_currentBufferSet];
    m_currentTraceIndex[m_currentBufferSet]++;  // increment for the future

    assert( currentIndex < c_maxTraceCount );
    if( currentIndex >= c_maxTraceCount )
        return -1;

    m_traceEntries[currentIndex][m_currentBufferSet].Name       = name;
    m_traceEntries[currentIndex][m_currentBufferSet].Depth      = m_recursionDepth;
    m_traceEntries[currentIndex][m_currentBufferSet].DisjointQ  = AllocQuery(false);
    m_traceEntries[currentIndex][m_currentBufferSet].BeginQ     = AllocQuery(true);
    m_traceEntries[currentIndex][m_currentBufferSet].EndQ       = AllocQuery(true);

    ID3D11DeviceContext* d3d11DeviceContext = m_renderContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext( );

    d3d11DeviceContext->Begin( m_traceEntries[currentIndex][m_currentBufferSet].DisjointQ.Get() );
    d3d11DeviceContext->End( m_traceEntries[currentIndex][m_currentBufferSet].BeginQ.Get( ) );

    m_recursionDepth++;
    return currentIndex;
#else
    m_recursionDepth++;
    return -1;
#endif
}

void vaGPUContextTracerDX11::End( int currentIndex )
{
    m_recursionDepth--;
    assert( m_recursionDepth >= 0 );
    if( currentIndex == -1 )
        return;
    if( !( currentIndex >= 0 && currentIndex < c_maxTraceCount ) )
    {
        assert( false );
        return;
    }

    ID3D11DeviceContext* d3d11DeviceContext = m_renderContext.SafeCast<vaRenderDeviceContextDX11*>( )->GetDXContext( );

    d3d11DeviceContext->End( m_traceEntries[currentIndex][m_currentBufferSet].EndQ.Get( ) );
    d3d11DeviceContext->End( m_traceEntries[currentIndex][m_currentBufferSet].DisjointQ.Get( ) );
}
