const std = @import("std");
const builtin = @import("builtin");
const io = @import("io/linux.zig");

const ArgParse = @import("arg_parse.zig");

const Lexer = @import("lexer.zig").Lexer;
const Parser = @import("parser.zig").Parser;
const SourceContext = @import("ast.zig").SourceContext;
const TypeCheck = @import("./type_check.zig");
const CodeGen = @import("./codegen/x64_gas_linux.zig");
const StringBuilder = @import("string_builder.zig");

const nob = @import("nob.zig");

pub fn build_asm_file(allocator: std.mem.Allocator, file_path: []const u8, out_path: []const u8, is_object_only: bool, object_files: std.array_list.Managed([]const u8)) !void {
    var cmd: nob.Cmd = .init(allocator);
    var temp_builder = StringBuilder.init(allocator);
    const obj_filename = try temp_builder.print_fmt("{s}.o", .{ file_path });

    try cmd.append_many(&[_][]const u8{ "as", "-g", file_path, "-o", if (is_object_only) out_path else obj_filename });

    var ret = try cmd.run();
    if (ret.Exited != 0) {
        std.log.err("failed to run command \"{s}\"!\n", .{try std.mem.join(allocator, " ", cmd.inner.items)});
        unreachable;
    }
    cmd.reset();
    if (is_object_only) return;

    try cmd.append_many(&[_][]const u8{ "gcc", "-g", obj_filename });
    for (object_files.items) |object_file| {
        try cmd.append(object_file);
    }
    try cmd.append_many( &[_][]const u8{"-o", out_path });
    ret = try cmd.run();
    if (ret.Exited != 0) {
        std.log.err("failed to run command \"{s}\"!\n", .{try std.mem.join(allocator, " ", cmd.inner.items)});
        unreachable;
    }
    cmd.reset();
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    var args: ArgParse = .{};

    args.init();
    try args.populate();

    std.debug.print("input file {s}\n", .{args.input_filename});

    const bytes = try io.read_entire_file(allocator, args.input_filename);

    var lexer: Lexer = undefined;

    const source_ctx = SourceContext.init(args.input_filename, bytes);

    lexer.init(allocator, source_ctx);

    try lexer.tokenize();

    var parser = Parser.init(allocator, lexer.tokens.items);
    var module = try parser.parse(source_ctx);

    var type_checker = TypeCheck.init(allocator, module.context);
    type_checker.type_check_mod(&module) catch |err| {
        std.log.debug("Error Occured {}", .{err});
        return;
    };
    var code_gen = CodeGen.init(allocator);
    try code_gen.compile_mod(&module);

    var buff: [1024]u8 = undefined;
    const asm_filename = try std.fmt.bufPrintZ(&buff, "{s}.s", .{args.output_filename});

    try io.write_entire_file(asm_filename, code_gen.program_builder.string.items);

    try build_asm_file(allocator, asm_filename, args.output_filename, args.object_only, args.link_object_filename);
}
