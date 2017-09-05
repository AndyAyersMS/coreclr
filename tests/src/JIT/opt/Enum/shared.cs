// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Some simple tests for the Enum.HasFlag.
// Verify the method throws the expected error for type mismatches.

using System;
using System.Runtime.CompilerServices;

class MyG<T,U> 
{
    public enum A 
    {
        X = 1
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public static void foo() 
    {
        var a = MyG<object,U>.A.X;
        a.HasFlag(MyG<T,string>.A.X);
    }
}

class My 
{
    public static int Main() 
    {
        int result = 0;
        try 
        {
            MyG<My,My>.foo();
        }
        catch(ArgumentException e)
        {
            string expected = "The argument type, 'MyG`2+A[My,System.String]', is not the same as the enum type 'MyG`2+A[System.Object,My]'.";
            if (expected.Equals(e.Message))
            {
                Console.WriteLine("Caught expected exception with correct message");
                Console.WriteLine(e.Message);
                result = 100;
            }
            else
            {
                Console.WriteLine("Caught expected exception, but message was incorrect");
                Console.WriteLine("Exception: {0}", e.Message);
                Console.WriteLine("Expected : {0}", expected);
                
            }
        }
        return result;
    }
}
