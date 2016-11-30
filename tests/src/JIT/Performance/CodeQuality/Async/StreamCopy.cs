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
    static int canaryIndex;
    static byte canary = 0x33;

    public static void Setup()
    {
        if (!isSetup)
        {
            byte[] source = new byte[size];
            sourceStream = new MemoryStream(source);
            destStream = File.Create(Path.GetTempFileName(), 4096, FileOptions.DeleteOnClose);
            canaryIndex = size/2;
            source[canaryIndex] = canary;
            isSetup = true;
        }
    }

    public static bool Validate()
    {
        destStream.Seek(canaryIndex, SeekOrigin.Begin);
        return (destStream.ReadByte() == canary);
    }

    public static int Main(string[] args)
    {
        Setup();
        bool copyResult = CopyTo(sourceStream, destStream);
        bool copyAsyncResult = CopyToAsync(sourceStream, destStream);
        return (copyResult && copyAsyncResult? 100 : -1);
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

    static bool CopyTo(Stream s, Stream d)
    {
        for (int j = 0; j < Iterations; j++)
        {
            s.CopyTo(d);
            d.Seek(0, SeekOrigin.Begin);
        }
        
        return Validate();
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

    static bool CopyToAsync(Stream s, Stream d)
    {
        for (int j = 0; j < Iterations; j++)
        {
            s.CopyToAsync(d);
            d.Seek(0, SeekOrigin.Begin);
        }

        return Validate();
    }
}
}
