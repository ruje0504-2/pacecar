using Pacecar.Sensors;
var kernel = new Guid("802ec45a-1e99-4b83-9920-87c98277ba9d");
var dxgi = new Guid("ca11c036-0102-4a2d-a6ad-f03cfed5d3c9");
if (!SwitchPresentSelector.Accept(kernel, 184) ||
    SwitchPresentSelector.Accept(dxgi, 42) || SwitchPresentSelector.Accept(kernel, 168))
    throw new Exception("present source mismatch");
var token = new byte[24];
System.Buffers.Binary.BinaryPrimitives.WriteUInt32LittleEndian(token, 255);
System.Buffers.Binary.BinaryPrimitives.WriteUInt64LittleEndian(token.AsSpan(8), 0x123456789);
if (!SwitchPresentSelector.TryGetSurface(0, token, out var surface) || surface != 0x123456789 ||
    SwitchPresentSelector.TryGetSurface(1, token, out _) ||
    SwitchPresentSelector.TryGetSurface(0, token.AsSpan(0, 12), out _))
    throw new Exception("GDI token layout or version mismatch");
Array.Clear(token, 0, 4);
if (SwitchPresentSelector.TryGetSurface(0, token, out _))
    throw new Exception("counted a non-pending update");
Console.WriteLine("PASS: kernel Present selection and versioned Win32k GDI update payload");
