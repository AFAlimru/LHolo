// Generates LHolo's Java block-state mapping directly from Chunker.
//
// Chunker is MIT licensed. See THIRD_PARTY_NOTICES.md in the LHolo repository.

// Compile/run against Chunker's shaded CLI jar. This is a development-time
// generator only; LHolo never embeds or launches a JVM at runtime.

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.hivemc.chunker.conversion.WorldConverter;
import com.hivemc.chunker.conversion.encoding.base.Version;
import com.hivemc.chunker.conversion.encoding.bedrock.base.resolver.identifier.BedrockBlockIdentifierResolver;
import com.hivemc.chunker.conversion.encoding.java.JavaDataVersion;
import com.hivemc.chunker.conversion.encoding.java.base.resolver.identifier.JavaBlockIdentifierResolver;
import com.hivemc.chunker.mapping.identifier.Identifier;

import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import java.util.UUID;
import java.util.zip.Deflater;

public final class GenerateMappings {
    private record JavaVersion(Path directory, Version version, int dataVersion) {}
    private record SeenOutput(int dataVersion, String output) {}

    private GenerateMappings() {}

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            throw new IllegalArgumentException("usage: GenerateMappings <Chunker root> <GeneratedChunkerMappings.inc>");
        }

        Path chunkerRoot = Path.of(args[0]).toAbsolutePath().normalize();
        String chunkerCommit = gitCommit(chunkerRoot);
        Path javaData = chunkerRoot.resolve("cli/data/java");
        Set<String> bedrockPermutations = loadBedrockPermutations(
                chunkerRoot.resolve("cli/data/bedrock/1.26.20.0/block_states.json")
        );
        Path output = Path.of(args[1]).toAbsolutePath().normalize();
        List<JavaVersion> versions = loadVersions(javaData);
        if (versions.isEmpty()) throw new IllegalStateException("No Chunker Java block data found in " + javaData);

        WorldConverter converter = new WorldConverter(UUID.randomUUID());
        BedrockBlockIdentifierResolver bedrock = new BedrockBlockIdentifierResolver(
                converter, new Version(1, 26, 20), false, false
        );

        // key is "javaName\0sortedJavaProperties". Each value records output
        // changes across Java data versions; identical repeated outputs collapse
        // when the final table is written.
        TreeMap<String, List<SeenOutput>> mappings = new TreeMap<>();
        long totalStates = 0;
        long unresolvedJava = 0;
        long unresolvedBedrock = 0;
        TreeMap<String, Integer> unresolvedBedrockInputs = new TreeMap<>();

        for (JavaVersion javaVersion : versions) {
            JavaBlockIdentifierResolver java = new JavaBlockIdentifierResolver(
                    converter, javaVersion.version(), true, false
            );
            JsonObject blocks = JsonParser.parseReader(Files.newBufferedReader(
                    javaVersion.directory().resolve("blocks.json"), StandardCharsets.UTF_8
            )).getAsJsonObject();

            long versionStates = 0;
            long versionUnresolvedJava = 0;
            long versionUnresolvedBedrock = 0;
            for (Map.Entry<String, JsonElement> blockEntry : blocks.entrySet()) {
                String javaName = blockEntry.getKey();
                JsonObject blockObject = blockEntry.getValue().getAsJsonObject();

                // Some real-world litematics omit Properties even for blocks
                // that normally have states. Ask Chunker for its canonical
                // defaults as an additional name-only input mapping.
                if (blockObject.has("properties") && !blockObject.getAsJsonObject("properties").isEmpty()) {
                    var defaultIntermediate = java.to(new Identifier(javaName));
                    if (defaultIntermediate.isPresent()) {
                        var defaultConverted = bedrock.from(defaultIntermediate.get());
                        defaultConverted.ifPresent(identifier -> recordMapping(
                                mappings, javaName + '\0', javaVersion.dataVersion(), identifier
                        ));
                    }
                }

                for (JsonElement stateElement : blockObject.getAsJsonArray("states")) {
                    JsonObject stateObject = stateElement.getAsJsonObject();
                    TreeMap<String, Object> properties = new TreeMap<>();
                    if (stateObject.has("properties")) {
                        for (Map.Entry<String, JsonElement> property
                                : stateObject.getAsJsonObject("properties").entrySet()) {
                            properties.put(property.getKey(), property.getValue().getAsString());
                        }
                    }

                    ++totalStates;
                    ++versionStates;
                    Optional<com.hivemc.chunker.conversion.intermediate.column.chunk.identifier.ChunkerBlockIdentifier>
                            intermediate = java.to(Identifier.fromBoxed(javaName, properties));
                    if (intermediate.isEmpty()) {
                        ++unresolvedJava;
                        ++versionUnresolvedJava;
                        continue;
                    }
                    Optional<Identifier> converted = bedrock.from(intermediate.get());
                    if (converted.isEmpty()) {
                        ++unresolvedBedrock;
                        ++versionUnresolvedBedrock;
                        unresolvedBedrockInputs.merge(
                                javaName + '[' + encodeProperties(properties) + ']', 1, Integer::sum
                        );
                        continue;
                    }

                    recordMapping(
                            mappings,
                            javaName + '\0' + encodeProperties(properties),
                            javaVersion.dataVersion(),
                            converted.get()
                    );
                }
            }
            System.out.printf("%s (DataVersion %d): %,d states, %,d unresolved Java, %,d unresolved Bedrock%n",
                    javaVersion.version(), javaVersion.dataVersion(), versionStates,
                    versionUnresolvedJava, versionUnresolvedBedrock);
        }

        Files.createDirectories(output.getParent());
        validateBedrockOutputs(mappings, bedrockPermutations);
        StringBuilder table = new StringBuilder(8 * 1024 * 1024);
        table.append("# Generated from Chunker commit ").append(chunkerCommit).append('\n');
        table.append("# Target Bedrock version: 1.26.20\n");
        table.append("# min_data_version\\tjava_name\\tjava_properties\\tbedrock_name\\tbedrock_states\n");
        for (Map.Entry<String, List<SeenOutput>> entry : mappings.entrySet()) {
            int separator = entry.getKey().indexOf('\0');
            String javaName = entry.getKey().substring(0, separator);
            String javaProperties = entry.getKey().substring(separator + 1);
            for (SeenOutput seen : entry.getValue()) {
                int outputSeparator = seen.output().indexOf('\0');
                table.append(seen.dataVersion()).append('\t')
                        .append(javaName).append('\t')
                        .append(javaProperties).append('\t')
                        .append(seen.output(), 0, outputSeparator).append('\t')
                        .append(seen.output(), outputSeparator + 1, seen.output().length())
                        .append('\n');
            }
        }
        byte[] rawTable = table.toString().getBytes(StandardCharsets.UTF_8);
        byte[] compressedTable = deflate(rawTable);
        writeCppInclude(output, rawTable.length, compressedTable, chunkerCommit);

        long transitions = mappings.values().stream().mapToLong(List::size).sum();
        long versionedKeys = mappings.values().stream().filter(history -> history.size() > 1).count();
        System.out.printf(
                "Done: %,d source states, %,d unique input states, %,d output records, "
                        + "%,d versioned keys, %,d unresolved Java, %,d unresolved Bedrock%n",
                totalStates, mappings.size(), transitions, versionedKeys, unresolvedJava, unresolvedBedrock
        );
        System.out.println("Output: " + output);
        if (!unresolvedBedrockInputs.isEmpty()) {
            System.out.println("Java states unsupported by the Bedrock 1.26.20 resolver:");
            unresolvedBedrockInputs.forEach((identifier, count) ->
                    System.out.printf("  %s (seen in %d source versions)%n", identifier, count));
        }
    }

    private static byte[] deflate(byte[] input) {
        Deflater deflater = new Deflater(Deflater.BEST_COMPRESSION);
        deflater.setInput(input);
        deflater.finish();
        ByteArrayOutputStream output = new ByteArrayOutputStream(input.length / 4);
        byte[] buffer = new byte[64 * 1024];
        while (!deflater.finished()) {
            output.write(buffer, 0, deflater.deflate(buffer));
        }
        deflater.end();
        return output.toByteArray();
    }

    private static void recordMapping(
            TreeMap<String, List<SeenOutput>> mappings,
            String key,
            int dataVersion,
            Identifier converted
    ) {
        String value = converted.getIdentifier() + '\0'
                + encodeProperties(new TreeMap<>(converted.getBoxedStates()));
        List<SeenOutput> history = mappings.computeIfAbsent(key, ignored -> new ArrayList<>());
        if (history.isEmpty() || !history.get(history.size() - 1).output().equals(value)) {
            history.add(new SeenOutput(dataVersion, value));
        }
    }

    private static void writeCppInclude(
            Path output, int rawSize, byte[] compressed, String chunkerCommit
    ) throws IOException {
        try (BufferedWriter writer = Files.newBufferedWriter(output, StandardCharsets.US_ASCII)) {
            writer.write("// Generated by tools/java_to_bedrock/GenerateMappings.java. Do not edit.\n");
            writer.write("// Chunker source commit: ");
            writer.write(chunkerCommit);
            writer.write('\n');
            writer.write("// Target Bedrock version: 1.26.20\n\n");
            writer.write("constexpr std::size_t kChunkerMappingRawSize = ");
            writer.write(Integer.toString(rawSize));
            writer.write(";\nconstexpr unsigned char kChunkerMappingCompressed[] = {\n    ");
            for (int i = 0; i < compressed.length; ++i) {
                writer.write(String.format("0x%02x", compressed[i] & 0xff));
                if (i + 1 != compressed.length) {
                    writer.write(',');
                    if ((i + 1) % 16 == 0) writer.write("\n    ");
                    else writer.write(' ');
                }
            }
            writer.write("\n};\n");
        }
        System.out.printf("Embedded table: %,d bytes raw, %,d bytes compressed%n", rawSize, compressed.length);
    }

    private static String gitCommit(Path chunkerRoot) throws IOException, InterruptedException {
        Process process = new ProcessBuilder(
                "git", "-C", chunkerRoot.toString(), "rev-parse", "HEAD"
        ).redirectErrorStream(true).start();
        String output;
        try (InputStream stream = process.getInputStream()) {
            output = new String(stream.readAllBytes(), StandardCharsets.UTF_8).trim();
        }
        if (process.waitFor() != 0 || output.length() != 40) {
            throw new IllegalStateException("Could not read the Chunker source commit: " + output);
        }
        return output;
    }

    private static List<JavaVersion> loadVersions(Path javaData) throws IOException {
        List<JavaVersion> versions = new ArrayList<>();
        try (var directories = Files.list(javaData)) {
            for (Path directory : directories.filter(Files::isDirectory).toList()) {
                Path blocks = directory.resolve("blocks.json");
                if (!Files.isRegularFile(blocks)) continue;
                Version version = Version.fromString(directory.getFileName().toString());
                versions.add(new JavaVersion(
                        directory,
                        version,
                        JavaDataVersion.getNearestVersion(version).getDataVersion()
                ));
            }
        }
        versions.sort(Comparator.comparing(JavaVersion::version));
        return versions;
    }

    private static Set<String> loadBedrockPermutations(Path blockStatesFile) throws IOException {
        Set<String> permutations = new HashSet<>();
        JsonObject root = JsonParser.parseReader(Files.newBufferedReader(
                blockStatesFile, StandardCharsets.UTF_8
        )).getAsJsonObject();
        for (JsonElement blockElement : root.getAsJsonArray("blocks")) {
            JsonObject block = blockElement.getAsJsonObject();
            TreeMap<String, Object> states = new TreeMap<>();
            for (JsonElement stateElement : block.getAsJsonArray("states")) {
                JsonObject state = stateElement.getAsJsonObject();
                JsonElement value = state.get("value");
                states.put(state.get("name").getAsString(),
                        value.isJsonPrimitive() && value.getAsJsonPrimitive().isString()
                                ? value.getAsString() : value.getAsInt());
            }
            permutations.add(block.get("name").getAsString() + '\0' + encodeProperties(states));
        }
        return permutations;
    }

    private static void validateBedrockOutputs(
            TreeMap<String, List<SeenOutput>> mappings,
            Set<String> bedrockPermutations
    ) {
        for (List<SeenOutput> history : mappings.values()) {
            for (SeenOutput seen : history) {
                int separator = seen.output().indexOf('\0');
                String name = seen.output().substring(0, separator);
                TreeMap<String, String> states = new TreeMap<>();
                String encodedStates = seen.output().substring(separator + 1);
                if (!encodedStates.isEmpty()) {
                    for (String property : encodedStates.split(",")) {
                        int equal = property.indexOf('=');
                        String key = property.substring(0, equal);
                        String value = property.substring(equal + 1);
                        if (key.equals("waterlogged")) continue;
                        if (value.equals("true")) value = "1";
                        else if (value.equals("false")) value = "0";
                        states.put(key, value);
                    }
                }
                String runtimeIdentifier = name + '\0' + encodeProperties(states);
                if (!bedrockPermutations.contains(runtimeIdentifier)) {
                    throw new IllegalStateException(
                            "Chunker produced a state absent from Bedrock 1.26.20 data: "
                                    + name + '[' + encodeProperties(states) + ']'
                    );
                }
            }
        }
        System.out.printf("Validated %,d generated output records against Bedrock 1.26.20 block states%n",
                mappings.values().stream().mapToLong(List::size).sum());
    }

    private static String encodeProperties(Map<String, ?> properties) {
        StringBuilder result = new StringBuilder();
        for (Map.Entry<String, ?> property : properties.entrySet()) {
            if (!result.isEmpty()) result.append(',');
            result.append(property.getKey()).append('=').append(property.getValue());
        }
        return result.toString();
    }
}
