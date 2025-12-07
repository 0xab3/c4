const std = @import("std");
const fs = std.fs;
const Allocator = std.mem.Allocator;
const IoError = fs.File.OpenError | fs.File.StatError;

pub fn create_file_if_not_exists(filename: []const u8) !fs.File {
    const file = if (fs.path.isAbsolute(filename))
        fs.openFileAbsolute(filename, .{ .mode = .read_write })
    else
        fs.cwd().openFile(filename, .{ .mode = .read_write });

    return file catch |err| switch (err) {
        fs.File.OpenError.FileNotFound => {
            return if (fs.path.isAbsolute(filename))
                try fs.createFileAbsolute(filename, .{})
            else
                try fs.cwd().createFile(filename, .{});
        },
        else => return err,
    };
}
pub fn read_entire_file(allocator: Allocator, filename: []const u8) ![]u8 {
    // @TODO(shahzad)!: this assumes that the file path is relative
    var file = if (fs.path.isAbsolute(filename))
        try fs.openFileAbsolute(filename, .{})
    else
        try fs.cwd().openFile(filename, .{});
    const stat = try file.stat();
    const size = stat.size;
    const memory = try allocator.alloc(u8, size);
    const bytes_read = try file.readAll(memory);
    std.debug.assert(bytes_read == size);
    return memory;
}
pub fn write_entire_file(filename: []const u8, buffer: []const u8) !void {
    var file = if (fs.path.isAbsolute(filename))
        try fs.createFileAbsolute(filename, .{})
    else
        try fs.cwd().createFile(filename, .{});
    try file.writeAll(buffer);
}
pub fn create_dir_if_not_exists(path: []const u8) !std.fs.Dir {
    const is_absolute = std.fs.path.isAbsolute(path);
    const dir = if (is_absolute)
        std.fs.openDirAbsolute(path, .{})
    else
        std.fs.cwd().openDir(path, .{});

    return dir catch |err| blk: switch (err) {
        error.FileNotFound => {
            if (is_absolute) {
                try std.fs.makeDirAbsolute(path);
                break :blk std.fs.openDirAbsolute(path, .{});
            } else {
                try std.fs.cwd().makeDir(path);
                break :blk std.fs.cwd().openDir(path, .{});
            }
        },
        else => {
            return err;
        },
    };
}
