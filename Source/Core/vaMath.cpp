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

#include "vaGeometry.h"
#include "vaMath.h"

using namespace Vanilla;

void vaSimple2DNoiseA::Initialize( int seed )
{
    vaRandom        random;
    random.Seed( seed );

    m_kMaxVertices = 256;
    m_kMaxVerticesMask = m_kMaxVertices - 1;
    delete[] m_r;
    m_r = new float[m_kMaxVertices * m_kMaxVertices];

    for( unsigned i = 0; i < m_kMaxVertices * m_kMaxVertices; ++i )
    {
        m_r[i] = random.NextFloat( );
    }
}

void vaSimple2DNoiseA::Destroy( )
{
    //BX_FREE( bgfx::g_allocator, m_r );
    delete[] m_r;
}

/// Evaluate the noise function at position x
float vaSimple2DNoiseA::Eval( const vaVector2& pt ) const
{
    // forgot to call Initialize?
    assert( m_r != NULL );

    if( m_r == NULL )
        return 0.0f;

    int xi = (int)floor( pt.x );
    int yi = (int)floor( pt.y );

    float tx = pt.x - xi;
    float ty = pt.y - yi;

    int rx0 = xi & m_kMaxVerticesMask;
    int rx1 = ( rx0 + 1 ) & m_kMaxVerticesMask;
    int ry0 = yi & m_kMaxVerticesMask;
    int ry1 = ( ry0 + 1 ) & m_kMaxVerticesMask;


    /// Random values at the corners of the cell
    float c00 = m_r[ry0 * m_kMaxVertices + rx0];
    float c10 = m_r[ry0 * m_kMaxVertices + rx1];
    float c01 = m_r[ry1 * m_kMaxVertices + rx0];
    float c11 = m_r[ry1 * m_kMaxVertices + rx1];

    /// Remapping of tx and ty using the Smoothstep function
    float sx = vaMath::Smoothstep( tx );
    float sy = vaMath::Smoothstep( ty );

    /// Linearly interpolate values along the x axis
    float nx0 = vaMath::Lerp( c00, c10, sx );
    float nx1 = vaMath::Lerp( c01, c11, sx );

    /// Linearly interpolate the nx0/nx1 along they y axis
    return vaMath::Lerp( nx0, nx1, sy );
}