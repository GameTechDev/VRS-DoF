/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTS:                     https://github.com/GameTechDev/GTS-GamesTaskScheduler
// GTS license:             MIT license
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#ifdef VA_GTS_INTEGRATION_ENABLED

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/ParallelReduce.h"

namespace Vanilla
{
    class vaGTS : public vaSingletonBase<vaGTS>
    {
        gts::WorkerPool                 m_workerPool;
        gts::MicroScheduler             m_microScheduler;

    protected:
        friend class vaCore;
        explicit vaGTS( int threadsToUse = 0 );
        ~vaGTS( );

    public:

        // no need to interface directly with the pool afaik?
        //gts::WorkerPool &           Pool()          { return m_workerPool; }
        gts::MicroScheduler &       Scheduler()     { return m_microScheduler; }
    };
}

#endif // VA_GTS_INTEGRATION_ENABLED
