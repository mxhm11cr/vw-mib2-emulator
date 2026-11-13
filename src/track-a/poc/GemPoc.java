import de.audi.gem.collections.HashtableOfLong;

/**
 * Track A PoC 1: execute AUTHENTIC firmware bytecode on Windows/JDK 21.
 * HashtableOfLong is an unmodified class from the P0369 GEM.jar
 * (de.audi.gem.collections, Java 1.2 bytecode, built 2015-10-19).
 * This harness is our own test code; the executed class is authentic VW/Audi HMI middleware.
 */
public class GemPoc {
    public static void main(String[] args) {
        Class<?> c = HashtableOfLong.class;
        System.out.println("[poc] authentic class: " + c.getName());
        System.out.println("[poc] code source: " + c.getProtectionDomain().getCodeSource().getLocation());
        System.out.println("[poc] classloader: " + c.getClassLoader());

        HashtableOfLong t = new HashtableOfLong(16);
        t.put(100L, "RADIO");
        t.put(200L, "MEDIA");
        t.put(300L, "PHONE");
        t.put(200L, "MEDIA_UPDATED");

        System.out.println("[poc] size()=" + t.size());
        System.out.println("[poc] get(200)=" + t.get(200L));
        System.out.println("[poc] get(999)=" + t.get(999L));
        System.out.println("[poc] containsKey(300)=" + t.containsKey(300L));
        System.out.println("[poc] containsKey(999)=" + t.containsKey(999L));
        System.out.println("[poc] toString()=" + t.toString());

        if (t.size() != 3 || !"MEDIA_UPDATED".equals(t.get(200L)) || t.get(999L) != null
                || !t.containsKey(300L) || t.containsKey(999L)) {
            System.out.println("[poc] FAIL: authentic class behavior incorrect");
            System.exit(1);
        }
        System.out.println("[poc] PASS: authentic GEM bytecode executed correctly on JDK 21");
    }
}
