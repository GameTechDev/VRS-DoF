/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Taskflow:                     https://github.com/cpp-taskflow/cpp-taskflow
// Taskflow license:             MIT license (https://github.com/cpp-taskflow/cpp-taskflow/blob/master/LICENSE)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#ifdef VA_TASKFLOW_INTEGRATION_ENABLED

#pragma warning ( push )
#pragma warning ( disable: 4267 4703 4701 )

#include <taskflow/taskflow.hpp>  // Cpp-Taskflow is header-only

#pragma warning ( pop )


namespace Vanilla
{


    class vaTF : public vaSingletonBase<vaTF>
    {
        tf::Executor                    m_executor;
        tf::ExecutorObserverInterface * m_observer = nullptr;

    protected:
        friend class vaCore;
        explicit vaTF( int threadsToUse = 0 );
        ~vaTF( );

    public:

        tf::Executor &                  Executor( )         { return m_executor; }

    private:
        bool                            IsTracing( )        { return m_observer != nullptr; }
        bool                            StartTracing( );
        bool                            StopTracing( string dumpFilePath );
    };
}

#endif // VA_TASKFLOW_INTEGRATION_ENABLED