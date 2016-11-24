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
    public const int Iterations = 1000;
#endif

    static int size = 8 * 4096;

    public static int Main(string[] args)
    {
        byte[] source = new byte[size];
        byte[] dest   = new byte[size];
        byte canary = 0xEE;
        int canaryIndex = size / 2;
        source[canaryIndex] = canary;
        MemoryStream sourceStream = new MemoryStream(source);
        MemoryStream destStream = new MemoryStream(dest, true);
        bool copyToResult = CopyTo(sourceStream, destStream, canaryIndex, canary);
        bool copyToAsyncResult = CopyToAsync(sourceStream, destStream, canaryIndex, canary);
        return (copyToResult && copyToAsyncResult ? 100 : -1);
    }

    [Benchmark]
    public static void TestCopyTo() 
    {
        byte[] source = new byte[size];
        byte[] dest   = new byte[size];
        byte canary = 0xEE;
        int canaryIndex = size / 2;
        source[canaryIndex] = canary;
        MemoryStream sourceStream = new MemoryStream(source);
        MemoryStream destStream = new MemoryStream(dest, true);

        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                for (int i = 0; i < Iterations; i++) {
                    CopyTo(sourceStream, destStream, canaryIndex, canary);
                }
            }
        }
    }

    static bool CopyTo(MemoryStream s, MemoryStream d, int i, int c)
    {
        for (int j = 0; j < Iterations; j++)
        {
            s.CopyTo(d);
        }

        d.Seek(i, SeekOrigin.Begin);
        return d.ReadByte() == c;
    }

    [Benchmark]
    public static void TestCopyToAsync() 
    {
        byte[] source = new byte[size];
        byte[] dest   = new byte[size];
        byte canary = 0xEE;
        int canaryIndex = size / 2;
        source[canaryIndex] = canary;
        MemoryStream sourceStream = new MemoryStream(source);
        MemoryStream destStream = new MemoryStream(dest, true);

        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                for (int i = 0; i < Iterations; i++) {
                    CopyToAsync(sourceStream, destStream, canaryIndex, canary);
                }
            }
        }
    }

    static bool CopyToAsync(MemoryStream s, MemoryStream d, int i, int c)
    {
        for (int j = 0; j < Iterations; j++)
        {
            s.CopyToAsync(d);
        }

        d.Seek(i, SeekOrigin.Begin);
        return d.ReadByte() == c;
    }
}
}
