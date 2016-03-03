// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Microsoft.Xunit.Performance;

[assembly: OptimizeForBenchmarks]
[assembly: MeasureInstructionsRetired]

public static class Test
{

#if DEBUG
    public const int Iterations = 1;
#else
    public const int Iterations = 10000000;
#endif

    // inline wrapper, loop
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int TestGetCurrentProcessorNumberWrapperLoop()
    {
        int result = 0;
        for (int i = 0; i < Iterations; i++)
        {
            result += TestGetCurrentProcessorNumberInlined();
        }
        return result;
    }

    [Benchmark]
    public static void TestWrapperLoop() {
        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                TestGetCurrentProcessorNumberWrapperLoop();
            }
        }
    }

    // No inline wrapper, loop
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int TestGetCurrentProcessorNumberNoWrapperLoop()
    {
        int result = 0;
        for (int i = 0; i < Iterations; i++)
        {
            result += GetCurrentProcessorNumber();
        }
        return result;
    }

    [Benchmark]
    public static void TestNoWrapperLoop() {
        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                TestGetCurrentProcessorNumberNoWrapperLoop();
            }
        }
    }

    // inline wrapper, no loop
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int TestGetCurrentProcessorNumberWrapperNoLoop()
    {
        int result = 0;
        result += TestGetCurrentProcessorNumberInlined();
        return result;
    }

    [Benchmark]
    public static void TestWrapperNoLoop() {
        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                for (int i = 0; i < Iterations; i++) {
                    TestGetCurrentProcessorNumberWrapperNoLoop();
                }
            }
        }
    }

    // no inline wrapper, no loop
    [MethodImpl(MethodImplOptions.NoInlining)]
    static int TestGetCurrentProcessorNumberNoWrapperNoLoop()
    {
        int result = 0;
        result += GetCurrentProcessorNumber();
        return result;
    }

    [Benchmark]
    public static void TestNoWrapperNoLoop() {
        foreach (var iteration in Benchmark.Iterations) {
            using (iteration.StartMeasurement()) {
                for (int i = 0; i < Iterations; i++) {
                    TestGetCurrentProcessorNumberNoWrapperNoLoop();
                }
            }
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    static int TestGetCurrentProcessorNumberInlined()
    {
        return GetCurrentProcessorNumber();
    }

    [DllImport("kernel32")]
    public static extern int GetCurrentProcessorNumber();

    public static int Main()
    {
        var s1 = Stopwatch.StartNew();
        TestGetCurrentProcessorNumberWrapperLoop();
        Console.WriteLine("WrapperLoop: {0}", s1.ElapsedMilliseconds);

        var s2 = Stopwatch.StartNew();
        TestGetCurrentProcessorNumberNoWrapperLoop();
        Console.WriteLine("NoWrapperLoop: {0}", s2.ElapsedMilliseconds);

        var s3 = Stopwatch.StartNew();
        for (int i = 0; i < Iterations; i++)
        {
            TestGetCurrentProcessorNumberWrapperNoLoop();
        }
        Console.WriteLine("WrapperNoLoop: {0}", s3.ElapsedMilliseconds);

        var s4 = Stopwatch.StartNew();
        for (int i = 0; i < Iterations; i++)
        {
            TestGetCurrentProcessorNumberNoWrapperNoLoop();
        }
        Console.WriteLine("NoWrapperNoLoop: {0}", s4.ElapsedMilliseconds);

        return 100;
    }
}



