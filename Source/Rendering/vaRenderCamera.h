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
#include "Scene/vaCameraBase.h"
#include "Core/Misc/vaResourceFormats.h"
#include "Rendering/vaRenderDeviceContext.h"

namespace Vanilla
{

    class vaRenderCamera : public vaCameraBase, public vaUIPanel, public vaRenderingModule
    {
    public:
        struct GeneralSettings
        {
            float   Exposure                        = 0.0f;             // EV100 - see https://google.github.io/filament/Filament.html#imagingpipeline/physicallybasedcamera
            float   ExposureCompensation            = 0.0f;             // added post-autoexposure - use for user exposure adjustment

            float   ExposureMin                     = -20.0f;           // [ -20.0, +20.0   ]   - danger, the UI expect these to be consecutive in memory 
            float   ExposureMax                     = 20.0f;            // [ -20.0, +20.0   ]   - danger, the UI expect these to be consecutive in memory 
            bool    UseAutoExposure                 = true;             // 
            float   AutoExposureAdaptationSpeed     = 10.0f;            // [   0.1, FLT_MAX ]   - use std::numeric_limits<float>::infinity() for instantenious; // 
            float   AutoExposureKeyValue            = 0.5f;             // [   0.0, 2.0     ]
            bool    UseAutoAutoExposureKeyValue     = true;             // 

            float   DefaultAvgLuminanceWhenDataNotAvailable = 0.5f;
        };
        struct TonemapSettings
        {
            bool    UseTonemapping                  = true;             // Just pass-through the values (with pre-exposed lighting) instead; luminance still gets computed
            float   Saturation                      = 1.0f;             // [   0.0, 5.0     ]
            bool    UseModifiedReinhard             = true;             // 
            float   ModifiedReinhardWhiteLevel      = 6.0f;             // [   0.0, FLT_MAX ]
        };
        struct BloomSettings
        {
            bool    UseBloom                        = false;
            float   BloomSize                       = 0.3f;             // the gaussian blur sigma used by the filter is BloomSize scaled by resolution
            float   BloomMultiplier                 = 0.05f;
            float   BloomMinThreshold               = 0.01f;             // ignore values below min threshold                                    (will get scaled with pre-exposure multiplier)
            float   BloomMaxClamp                   = 200.0f;           // never transfer more than this amount of color to neighboring pixels  (will get scaled with pre-exposure multiplier)
        };
        struct DepthOfFieldSettings
        {
            bool    UseDOF                          = false;
        };

        struct AllSettings
        {
            GeneralSettings                         GeneralSettings;
            TonemapSettings                         TonemapSettings;
            BloomSettings                           BloomSettings;
            DepthOfFieldSettings                    DoFSettings;
        };

        AllSettings                                 m_settings;

        static const int                            c_backbufferCount = vaRenderDevice::c_BackbufferCount+1;          // also adds c_backbufferCount-1 lag!

    private:
        int                                         m_avgLuminancePrevLastWrittenIndex;
        shared_ptr<vaTexture>                       m_avgLuminancePrevCPU[c_backbufferCount];
        bool                                        m_avgLuminancePrevCPUHasData[c_backbufferCount];
        float                                       m_avgLuminancePrevCPUPreExposure[c_backbufferCount];
        float                                       m_lastAverageLuminance;

        bool const                                  m_visibleInUI;

    public:
        vaRenderCamera( vaRenderDevice & renderDevice, bool visibleInUI );
        virtual ~vaRenderCamera( );
        
        // copying not supported for now (although vaCameraBase should be able to get all info from vaRenderCamera somehow?)
        vaRenderCamera( const vaRenderCamera & other )                  = delete;
        vaRenderCamera & operator = ( const vaRenderCamera & other )    = delete;

        AllSettings &                               Settings()              { return m_settings; }
        GeneralSettings      &                      GeneralSettings()       { return m_settings.GeneralSettings; }
        TonemapSettings      &                      TonemapSettings()       { return m_settings.TonemapSettings; }
        BloomSettings        &                      BloomSettings()         { return m_settings.BloomSettings; }
        DepthOfFieldSettings &                      DepthOfFieldSettings()  { return m_settings.DoFSettings; }

        const AllSettings &                         Settings() const        { return m_settings; }

        // this gets called by tonemapping to provide the last luminance data as a GPU-based texture; if called multiple times
        // between PreRenderTick calls, only the last value will be used
        void                                        UpdateLuminance( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & inputLuminance );
        
        // this has to be called before starting any rendering to setup exposure and any related params; 
        // it also expects that vaCameraBase::Tick was called before to handle matrix updates and similar
        void                                        PreRenderTick( vaRenderDeviceContext & renderContext, float deltaTime, bool alwaysUseDefaultLuminance = false );

        // if determinism is required between changing of scenes or similar
        void                                        ResetHistory( );

        virtual float                               GetEV100( bool includeExposureCompensation ) const override;

    public:
    protected:
        // vaUIPanel
        // virtual string                              UIPanelGetDisplayName( ) const;
        virtual bool                                UIPanelIsListed( ) const { return m_visibleInUI; }
        virtual void                                UIPanelTick( vaApplicationBase & application ) override;
    };
    
}
