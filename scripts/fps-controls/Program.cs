using Pacecar.Sensors;
var kernel = new Guid("802ec45a-1e99-4b83-9920-87c98277ba9d");
var dxgi = new Guid("ca11c036-0102-4a2d-a6ad-f03cfed5d3c9");
if (!SwitchPresentSelector.Accept(kernel, 184) ||
    SwitchPresentSelector.Accept(dxgi, 42) || SwitchPresentSelector.Accept(kernel, 168))
    throw new Exception("present source mismatch");
Console.WriteLine("PASS: one kernel Present source; DXGI/flip events excluded (selector only)");
