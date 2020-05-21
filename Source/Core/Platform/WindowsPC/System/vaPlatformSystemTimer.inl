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

#include "vaPlatformBase.h"


namespace Vanilla
{
   inline vaSystemTimer::vaSystemTimer()
   {
      m_platformData.StartTime.QuadPart      = 0;
      m_platformData.CurrentTime.QuadPart    = 0;
      m_platformData.CurrentDelta.QuadPart   = 0;

      // this should be static and initialized only once really!
      ::QueryPerformanceFrequency( &m_platformData.QPF_Frequency );

      m_isRunning = false;
   }

   inline vaSystemTimer::~vaSystemTimer()
   {
   }

   inline void vaSystemTimer::Start( )
   {
      assert( !m_isRunning );
      m_isRunning = true;

      QueryPerformanceCounter( &m_platformData.StartTime );
      m_platformData.CurrentTime = m_platformData.StartTime;
      m_platformData.CurrentDelta.QuadPart   = 0;
   }

   inline void vaSystemTimer::Stop( )
   {
      assert( m_isRunning );
      m_isRunning = false;

      m_platformData.StartTime.QuadPart      = 0;
      m_platformData.CurrentTime.QuadPart    = 0;
      m_platformData.CurrentDelta.QuadPart   = 0;
   }

   inline void vaSystemTimer::Tick( )
   {
      if( !m_isRunning )
         return;

      LARGE_INTEGER currentTime;
      QueryPerformanceCounter( &currentTime );
      
      m_platformData.CurrentDelta.QuadPart = currentTime.QuadPart - m_platformData.CurrentTime.QuadPart;
      m_platformData.CurrentTime = currentTime;

   }

   inline double vaSystemTimer::GetTimeFromStart( ) const
   {
      LARGE_INTEGER timeFromStart;
      timeFromStart.QuadPart = m_platformData.CurrentTime.QuadPart - m_platformData.StartTime.QuadPart;

      return timeFromStart.QuadPart / (double) m_platformData.QPF_Frequency.QuadPart;
   }

   inline double vaSystemTimer::GetDeltaTime( ) const
   {
      return m_platformData.CurrentDelta.QuadPart / (double) m_platformData.QPF_Frequency.QuadPart;
   }

   inline double vaSystemTimer::GetCurrentTimeDouble( ) const
   {
       LARGE_INTEGER currentTime;
       QueryPerformanceCounter( &currentTime );

       return currentTime.QuadPart / (double) m_platformData.QPF_Frequency.QuadPart;
   }

}