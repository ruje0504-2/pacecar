using System.Buffers.Binary;

namespace Pacecar.Sensors;

internal static class SwitchPresentSelector
{
    internal static readonly Guid Kernel = new("802ec45a-1e99-4b83-9920-87c98277ba9d");
    internal static readonly Guid Win32k = new("8c416c79-d49b-4f01-a467-e56d3aa8234c");
    internal const int KernelPresent = 184;
    internal const int GdiSysMemToken = 33;
    internal const int GdiPayloadSize = 24;

    public static bool Accept(Guid provider, int id) => provider == Kernel && id == KernelPresent;

    // Win32k GdiSysMemToken v0 contains two UInt32s and two UInt64s on both
    // WOW64 and ARM64. The pending update identifies its logical GDI surface.
    internal static bool TryGetSurface(int version, ReadOnlySpan<byte> payload, out ulong surface)
    {
        surface = 0;
        if (version != 0 || payload.Length != GdiPayloadSize ||
            BinaryPrimitives.ReadUInt32LittleEndian(payload) == 0)
            return false;

        surface = BinaryPrimitives.ReadUInt64LittleEndian(payload[8..]);
        return surface != 0;
    }
}
