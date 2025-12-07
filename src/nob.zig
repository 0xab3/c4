const std = @import("std");
const io = @import("io/linux.zig");
const assert = std.debug.assert;
const ChildProcess = std.process.Child;
const Allocator = std.mem.Allocator;
const ArrayListManaged = std.array_list.Managed;

pub const Cmd = struct {
    const RunOptions = struct {
        stdin_path: ?[]const u8 = null,
        stdout_path: ?[]const u8 = null,
        stderr_path: ?[]const u8 = null,
    };
    stdin: ?std.fs.File = null,
    stdout: ?std.fs.File = null,
    stderr: ?std.fs.File = null,
    allocator: Allocator,
    arg_list: ArrayListManaged([]const u8),
    proc: ChildProcess = undefined,

    const Self = @This();
    pub fn init(allocator: Allocator) Self {
        return Self{
            .allocator = allocator,
            .arg_list = .init(allocator),
        };
    }
    pub fn append_many(self: *Self, items: []const []const u8) !void {
        return self.arg_list.appendSlice(items);
    }
    pub fn append(self: *Self, item: []const u8) !void {
        return self.arg_list.append(item);
    }
    pub fn run(self: *Self, opts: RunOptions) !std.posix.WaitPidResult {
        const cmd_as_str = try std.mem.join(self.allocator, " ", self.arg_list.items);
        std.log.info("CMD: {s}", .{cmd_as_str});
        const cpid = try std.posix.fork();

        self.stdin = if (opts.stdin_path) |stdin| (try io.create_file_if_not_exists(stdin)) else self.stdin;
        self.stdout = if (opts.stdout_path) |stdout| (try io.create_file_if_not_exists(stdout)) else self.stdout;
        self.stderr = if (opts.stderr_path) |stderr| (try io.create_file_if_not_exists(stderr)) else self.stderr;
        // we are the child
        if (cpid == 0) {
            if (self.stdin) |file| try std.posix.dup2(file.handle, std.posix.STDIN_FILENO);
            if (self.stdout) |file| try std.posix.dup2(file.handle, std.posix.STDOUT_FILENO);
            if (self.stderr) |file| try std.posix.dup2(file.handle, std.posix.STDERR_FILENO);

            const ret = std.process.execv(self.allocator, self.arg_list.items);

            std.log.err("failed to execv process '{s}': {}", .{ cmd_as_str, ret });
            unreachable; // we have replaced the process so this is unreachable
        }
        return std.posix.waitpid(cpid, 0);
    }
    pub fn reset(self: *Self) void {
        self.arg_list.clearRetainingCapacity();
    }
};
