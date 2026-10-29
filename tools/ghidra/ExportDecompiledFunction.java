// Export one selected function's Ghidra decompiler view outside the repository.
//
// Run with analyzeHeadless -postScript ExportDecompiledFunction.java
// <runtime-address> <new-absolute-output>. Generated text is original-derived
// analysis output and must remain outside the checkout and supplied media.

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

public final class ExportDecompiledFunction extends GhidraScript {
    private static boolean isExternalAnalysisOutput(Path output) {
        if (!output.isAbsolute() || Files.exists(output) || Files.isSymbolicLink(output)) {
            return false;
        }
        final Path normalized = output.normalize();
        final String text = normalized.toString();
        return !text.startsWith("/tmp/") && !text.startsWith("/private/tmp/")
            && (text.contains("/.cache/project-eon-analysis/")
                || text.contains("/project-eon-ghidra-analysis/"));
    }

    @Override
    public void run() throws Exception {
        final String[] arguments = getScriptArgs();
        if (arguments.length != 2) {
            throw new IllegalArgumentException(
                "expected runtime address and new external analysis output path");
        }
        final Address entry = currentProgram.getAddressFactory().getAddress(arguments[0]);
        if (entry == null) {
            throw new IllegalArgumentException("invalid runtime address");
        }
        final Path output = Path.of(arguments[1]);
        if (!isExternalAnalysisOutput(output)) {
            throw new IllegalArgumentException("output must be a new external analysis file");
        }
        final Function existing = currentProgram.getFunctionManager().getFunctionAt(entry);
        final Function function = existing != null ? existing : createFunction(entry, null);
        if (function == null) {
            throw new IOException("could not create a function at the selected entry");
        }
        final DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        final DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
        if (!result.decompileCompleted() || result.getDecompiledFunction() == null) {
            throw new IOException("Ghidra did not produce a complete decompilation");
        }
        Files.writeString(output, result.getDecompiledFunction().getC(), StandardCharsets.UTF_8,
            StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE);
        println("runtime=" + entry + " output=" + output + " bytes=" + Files.size(output));
    }
}
