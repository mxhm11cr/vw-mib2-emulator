// Track A A3 verification harness: load converted classes on the JVM and distinguish
// conversion bugs (VerifyError / ClassFormatError) from missing-runtime dependencies
// (NoClassDefFoundError / ExceptionInInitializerError). Loads without initialization
// (Class.forName initialize=false) to trigger linking/verification without clinit side effects.
import java.net.*;
import java.nio.file.*;
import java.util.*;

public class VerifyHarness {
    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args[0]);
        URLClassLoader loader = new URLClassLoader(new URL[]{root.toUri().toURL()}, VerifyHarness.class.getClassLoader());
        List<String> names = new ArrayList<>();
        Files.walk(root).filter(p -> p.toString().endsWith(".class")).forEach(p -> {
            String rel = root.relativize(p).toString().replace('\\', '/');
            names.add(rel.substring(0, rel.length() - 6).replace('/', '.'));
        });
        Collections.sort(names);
        int ok = 0, verifyFail = 0, depFail = 0, skip = 0;
        List<String> vf = new ArrayList<>(), df = new ArrayList<>();
        Map<String, int[]> pkg = new TreeMap<>();
        for (String name : names) {
            if (name.startsWith("java.") || name.startsWith("jdk.") || name.startsWith("sun.")) { skip++; continue; }
            String group = name.startsWith("de.vw") ? "de.vw" : name.startsWith("tsd.") ? "tsd" : name.startsWith("com.ibm.j9") ? "com.ibm.j9" : name.startsWith("com.ibm") ? "com.ibm.other" : "other";
            int[] st = pkg.computeIfAbsent(group, k -> new int[2]);
            try {
                Class.forName(name, false, loader);
                ok++; st[0]++;
            } catch (VerifyError | ClassFormatError e) {
                verifyFail++; st[1]++;
                if (vf.size() < 40) vf.add(name + ": " + e.getClass().getSimpleName() + " " + e.getMessage());
            } catch (Throwable t) {
                depFail++;
                if (df.size() < 10) df.add(name + ": " + t.getClass().getSimpleName());
            }
        }
        System.out.println("VERIFY ok=" + ok + " verifyFail=" + verifyFail + " depFail=" + depFail + " skip(java.*)=" + skip + " total=" + names.size());
        System.out.println("-- per-package [ok/fail] --");
        for (Map.Entry<String, int[]> e : pkg.entrySet()) System.out.println("  " + e.getKey() + " [" + e.getValue()[0] + "/" + e.getValue()[1] + "]");
        System.out.println("-- verification failures (conversion bugs) --");
        for (String s : vf) System.out.println("  " + s);
        System.out.println("-- dependency failures (sample) --");
        for (String s : df) System.out.println("  " + s);
    }
}
