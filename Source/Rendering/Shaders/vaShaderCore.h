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

#ifndef VA_SHADER_CORE_H
#define VA_SHADER_CORE_H

#ifndef VA_COMPILED_AS_SHADER_CODE

#include "Core/vaCoreIncludes.h"

#else

#include "MagicMacrosMagicFile.h"

#define vaMatrix4x4 float4x4
#define vaVector4   float4
#define vaVector3   float3
#define vaVector2   float2
#define vaVector2i  int2
#define vaVector4i  int4

#define PASTE1(a, b) a##b
#define PASTE(a, b) PASTE1(a, b)

#define B_CONCATENATER(x) PASTE(b,x)
#define S_CONCATENATER(x) PASTE(s,x)
#define T_CONCATENATER(x) PASTE(t,x)
#define U_CONCATENATER(x) PASTE(u,x)

#endif

#endif // VA_SHADER_CORE_H