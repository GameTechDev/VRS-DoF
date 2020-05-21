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

#include "vaScene.h"

namespace Vanilla
{

    class vaSceneTools
    {
    public:
        template< typename InArrayElementType, typename OutArrayElementType >
        static int FrustumCull( InArrayElementType * arrayIn, size_t arrayInCount, std::vector<OutArrayElementType> & arrayOut, vaPlane frustumPlanes[], const int planeCount = 6 );
    };


    template< typename InArrayElementType, typename OutArrayElementType >
    inline int vaSceneTools::FrustumCull( InArrayElementType * arrayIn, size_t arrayInCount, std::vector<OutArrayElementType> & arrayOut, vaPlane frustumPlanes[], const int planeCount )
    {
        int addedCount = 0;

        for( size_t i = 0; i < arrayInCount; i++ )
        {
            vaOrientedBoundingBox obb;
            arrayIn[i]->GetBounds( obb );

            if( obb.IntersectFrustum( frustumPlanes, planeCount ) )
            {
                addedCount++;
                arrayOut.push_back( static_cast<OutArrayElementType>(arrayIn[i]) );
            }
        }

        return addedCount;
    }

}