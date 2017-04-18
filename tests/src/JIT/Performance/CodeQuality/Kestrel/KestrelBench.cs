using System;
using System.Diagnostics;
using Xunit;
using Microsoft.Xunit.Performance;

[assembly: OptimizeForBenchmarks]
[assembly: MeasureInstructionsRetired]

class Driver
{
    public static int Main()
    {
        RequestParsingBenchmark rpb = new RequestParsingBenchmark();

        rpb.Setup();
        rpb.PlaintextTechEmpower();  // warmup

        Stopwatch sw = Stopwatch.StartNew();
        rpb.PlaintextTechEmpower();  // warmup
        sw.Stop();

        // Print result.
        string name = "PlaintextTechEmpower";
        double timeInMs = sw.Elapsed.TotalMilliseconds;
        Console.Write("{0,25}: {1,7:F2}ms", name, timeInMs);

        return 100;
    }
}
