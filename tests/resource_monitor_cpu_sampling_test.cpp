#include "resourcemonitormath.h"

#include <cmath>
#include <cstdio>

int main()
{
#ifdef __sg_public
    const sg_error initStatus = sg_init(0);
#else
    const sg_error initStatus = sg_init();
#endif
    if (initStatus != SG_ERROR_NONE)
    {
        std::fprintf(stderr, "sg_init failed: %s\n", sg_str_error(initStatus));
        return 1;
    }

    (void)sg_drop_privileges();

    const ResourceMonitorMath::CpuUsageSample usage = ResourceMonitorMath::sampleCpuUsage();

    int result = 0;
    if (!usage.valid)
    {
        std::fprintf(stderr,
                     "libstatgrab returned no CPU sample (entries=%zu): %s\n",
                     usage.reportedEntries,
                     sg_str_error(sg_get_error()));
        result = 1;
    }
    else if (usage.reportedEntries == 0)
    {
        std::printf("accepted non-null CPU sample with reported entries=0 (%.2f%%)\n", usage.percent);
    }
    else
    {
        std::printf("accepted CPU sample with reported entries=%zu (%.2f%%)\n",
                    usage.reportedEntries,
                    usage.percent);
    }

    if (!std::isfinite(usage.percent) || usage.percent < 0.0 || usage.percent > 100.0)
    {
        std::fprintf(stderr, "CPU sample percentage is out of range: %f\n", usage.percent);
        result = 1;
    }

    sg_shutdown();
    return result;
}
