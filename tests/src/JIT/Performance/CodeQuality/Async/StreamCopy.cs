// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Microsoft.Xunit.Performance;
using System;
using System.IO;

[assembly: OptimizeForBenchmarks]
[assembly: MeasureInstructionsRetired]

namespace Async
{

public class StreamCopy
{

#if DEBUG
    public const int Iterations = 1;
#else
    public const int Iterations = 1000 * 1000;
#endif

    static int size = 8 * 4096;
    static Stream sourceStream;
    static Stream destStream;
    static bool isSetup = false;

    public static void Setup()
    {
        if (!isSetup)
        {
            byte[] source = new byte[size];
            sourceStream = new MemoryStream(source);
            destStream = File.Create(Path.GetTempFileName(), 4096, FileOptions.DeleteOnClose);
            isSetup = true;
        }
    }

    public static int Main(string[] args)
    {
        Setup();
        CopyTo(sourceStream, destStream);
        CopyToAsync(sourceStream, destStream);
        return 100;
    }

    [Benchmark]
    public static void TestCopyTo() 
    {
        Setup();

        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                CopyTo(sourceStream, destStream);
            }
        }
    }

    static void CopyTo(Stream s, Stream d)
    {
        for (int j = 0; j < Iterations; j++)
        {
            s.CopyTo(d);
            d.Seek(0, SeekOrigin.Begin);
        }
    }

    [Benchmark]
    public static void TestCopyToAsync() 
    {
        Setup();

        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                CopyToAsync(sourceStream, destStream);
            }
        }
    }

    static void CopyToAsync(Stream s, Stream d)
    {
        for (int j = 0; j < Iterations; j++)
        {
            s.CopyToAsync(d);
            d.Seek(0, SeekOrigin.Begin);
        }
    }
}
}
