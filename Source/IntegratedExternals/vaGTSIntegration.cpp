/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTS:                     https://github.com/GameTechDev/GTS-GamesTaskScheduler
// GTS license:             MIT license
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "IntegratedExternals/vaGTSIntegration.h"

#include "Core/Misc/vaProfiler.h"

#ifdef VA_GTS_INTEGRATION_ENABLED

using namespace Vanilla;

vaGTS::vaGTS( int threadsToUse )// : m_shuttingDown( false )
{
    bool result = m_workerPool.initialize( threadsToUse );
    GTS_ASSERT(result);
    result = m_microScheduler.initialize(&m_workerPool);
    GTS_ASSERT(result);
}

vaGTS::~vaGTS( )
{
    m_microScheduler.shutdown();
    m_workerPool.shutdown();
}

#endif