const std = @import("std");
const Allocator = std.mem.Allocator;
const ArrayListManaged = std.array_list.Managed;

pub const Cmd = struct {
    allocator: Allocator,
    inner: ArrayListManaged([]const u8),
    const Self = @This();
    pub fn init(allocator: Allocator) Self {
        return Self{
            .allocator = allocator,
            .inner = .init(allocator),
        };
    }
    pub fn append_many(self: *Self, items: []const []const u8) !void {
        return self.inner.appendSlice(items);
    }
    pub fn append(self: *Self, item: []const u8) !void {
        return self.inner.append(item);
    }
    pub fn run(self: *Self) !std.process.Child.Term {
        var cp = std.process.Child.init(self.inner.items, self.allocator);
        std.log.info("CMD: {s}", .{try std.mem.join(self.allocator, " ", self.inner.items)});
        return cp.spawnAndWait();
    }
    pub fn reset(self: *Self) void {
        self.inner.clearRetainingCapacity();
    }
};
