using System.Security.Principal;

using Microsoft.Diagnostics.Tracing.Session;

namespace Pacecar.Sensors;

/// <summary>
/// Entry point for the optional elevated sensor helper. Launched on demand by the UI (via runas)
/// when the user enables deep sensors; its manifest requests administrator so that produces a single
/// UAC prompt. The pipe protocol is identical regardless of deployment, so a Windows service
/// (LocalSystem) can be added later without protocol changes: the service would call
/// <see cref="PipeServer.BuildPipeName"/> with the installing user's SID.
/// </summary>
internal static class Program
{
    private static int Main(string[] args)
    {
        // Load helper DLLs (LibreHardwareMonitor, HidSharp, PawnIO client) only from System32/known
        // locations rather than the current working directory, to stop DLL search-order hijacking in
        // this elevated process.
        _ = NativeMethods.SetDefaultDllDirectories(NativeMethods.LoadLibrarySearchSystem32);

        var userSid = WindowsIdentity.GetCurrent().User?.Value ?? string.Empty;
        var pipeName = PipeServer.BuildPipeName(userSid);
        for (var i = 0; i < args.Length - 1; i++)
        {
            if (args[i] == "--pipe")
            {
                pipeName = args[i + 1];
            }
        }

#if !PACECAR_SWITCH
        var pawnIoPresent = PawnIOProbe.IsPresent();
        Console.WriteLine($"[main] PawnIO: {(pawnIoPresent ? "present" : "absent")}");
        Console.WriteLine($"[main] {PawnIOProbe.Guidance(pawnIoPresent)}");

#endif
        HelperLog.Write($"[main] starting; pid={Environment.ProcessId} elevated={TraceEventSession.IsElevated() == true}");
        HelperLog.Write($"[main] pipe={pipeName}");

#if !PACECAR_SWITCH
        using var sensors = new SensorServer();
        sensors.Open();
#endif

        using var cancellation = new CancellationTokenSource();
        Console.CancelKeyPress += (_, eventArgs) =>
        {
            eventArgs.Cancel = true;
            cancellation.Cancel();
        };

        using var frameTime = new EtwFrameTime();
        var server = new PipeServer(pipeName, userSid, frameTime);
        Console.WriteLine($"[main] listening on {pipeName}");
#if PACECAR_SWITCH
        // SoC sensors are read by the native UI from SwitchFan. This ARM64 helper
        // owns only the opt-in ETW session; no x86 PawnIO/LHM driver is loaded.
        server.Run(() => Array.Empty<IpcProtocol.Reading>(), TimeSpan.FromMilliseconds(250), cancellation.Token);
#else
        server.Run(sensors.Read, TimeSpan.FromSeconds(1), cancellation.Token);
#endif
        Console.WriteLine("[main] stopped");
        return 0;
    }
}