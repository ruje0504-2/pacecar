namespace Pacecar.Sensors;
// PresentMon f50c407ad9492051f4b834d77c31c49f38f0fef0:
// Microsoft_Windows_DxgKrnl::Present_Info descriptor 0x00b8.
// This measures the target's kernel Present submission cadence, not panel refresh
// or guaranteed displayed frames. Uninstrumented paths report no frames.
internal static class SwitchPresentSelector {
    private static readonly Guid Kernel=new("802ec45a-1e99-4b83-9920-87c98277ba9d");
    public static bool Accept(Guid provider,int id) => provider==Kernel && id==184;
}
