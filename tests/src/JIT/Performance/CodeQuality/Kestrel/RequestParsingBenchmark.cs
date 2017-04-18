using System;
using Microsoft.AspNetCore.Server.Kestrel.Core;
using Microsoft.AspNetCore.Server.Kestrel.Core.Internal;
using Microsoft.AspNetCore.Server.Kestrel.Core.Internal.Http;
using Microsoft.AspNetCore.Server.Kestrel.Internal.System.IO.Pipelines;

using Microsoft.AspNetCore.Server.Kestrel.Performance;
using Microsoft.AspNetCore.Server.Kestrel.Performance.Mocks;

using Xunit;
using Microsoft.Xunit.Performance;

public class RequestParsingBenchmark
{
    public IPipe Pipe { get; set; }
    
    public Frame<object> Frame { get; set; }
    
    public PipeFactory PipelineFactory { get; set; }
    
    public void Setup()
    {
        if (Pipe != null)
        {
            return;
        }

        var serviceContext = new ServiceContext
        {
            HttpParserFactory = f => new HttpParser<FrameAdapter>(f.Frame.ServiceContext.Log),
            ServerOptions = new KestrelServerOptions()
        };
        var frameContext = new FrameContext
        {
            ServiceContext = serviceContext,
            ConnectionInformation = new MockConnectionInformation()
        };
        
        Frame = new Frame<object>(application: null, frameContext: frameContext);
        Frame.TimeoutControl = new MockTimeoutControl();
        PipelineFactory = new PipeFactory();
        Pipe = PipelineFactory.Create();
    }

    [Benchmark]
    public void PlaintextTechEmpower()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.PlaintextTechEmpowerRequest);
            ParseData();
        }
    }

    [Benchmark]
    public void PlaintextAbsoluteUri()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.PlaintextAbsoluteUriRequest);
            ParseData();
        }
    }

    [Benchmark]
    public void PipelinedPlaintextTechEmpower()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.PlaintextTechEmpowerPipelinedRequests);
            ParseData();
        }
    }

    [Benchmark]
    public void PipelinedPlaintextTechEmpowerDrainBuffer()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.PlaintextTechEmpowerPipelinedRequests);
            ParseDataDrainBuffer();
        }
    }

    [Benchmark]
    public void LiveAspNet()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.LiveaspnetRequest);
            ParseData();
        }
    }

    [Benchmark]
    public void PipelinedLiveAspNet()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.LiveaspnetPipelinedRequests);
            ParseData();
        }
    }

    [Benchmark]
    public void Unicode()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.UnicodeRequest);
            ParseData();
        }
    }

    [Benchmark]
    public void UnicodePipelined()
    {
        for (var i = 0; i < RequestParsingData.InnerLoopCount; i++)
        {
            InsertData(RequestParsingData.UnicodePipelinedRequests);
            ParseData();
        }
    }

    private void InsertData(byte[] bytes)
    {
        var buffer = Pipe.Writer.Alloc(2048);
        buffer.Write(bytes);
        // There should not be any backpressure and task completes immediately
        buffer.FlushAsync().GetAwaiter().GetResult();
    }
    
    private void ParseDataDrainBuffer()
    {
        var awaitable = Pipe.Reader.ReadAsync();
        if (!awaitable.IsCompleted)
        {
            // No more data
            return;
        }
        
        var readableBuffer = awaitable.GetResult().Buffer;
        do
        {
            Frame.Reset();
            
            if (!Frame.TakeStartLine(readableBuffer, out var consumed, out var examined))
            {
                ErrorUtilities.ThrowInvalidRequestLine();
            }
            
            readableBuffer = readableBuffer.Slice(consumed);
            
            Frame.InitializeHeaders();
            
            if (!Frame.TakeMessageHeaders(readableBuffer, out consumed, out examined))
            {
                ErrorUtilities.ThrowInvalidRequestHeaders();
            }
            
            readableBuffer = readableBuffer.Slice(consumed);
        }
        while (readableBuffer.Length > 0);
        
        Pipe.Reader.Advance(readableBuffer.End);
    }
    
    private void ParseData()
    {
        do
        {
            var awaitable = Pipe.Reader.ReadAsync();
            if (!awaitable.IsCompleted)
            {
                // No more data
                return;
            }
            
            var result = awaitable.GetAwaiter().GetResult();
            var readableBuffer = result.Buffer;
            
            Frame.Reset();
            
            if (!Frame.TakeStartLine(readableBuffer, out var consumed, out var examined))
            {
                ErrorUtilities.ThrowInvalidRequestLine();
            }
            Pipe.Reader.Advance(consumed, examined);
            
            result = Pipe.Reader.ReadAsync().GetAwaiter().GetResult();
            readableBuffer = result.Buffer;
            
            Frame.InitializeHeaders();
            
            if (!Frame.TakeMessageHeaders(readableBuffer, out consumed, out examined))
            {
                ErrorUtilities.ThrowInvalidRequestHeaders();
            }
            Pipe.Reader.Advance(consumed, examined);
        }
        while (true);
    }
}
