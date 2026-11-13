import de.vw.extlibs.collections.ints.IntObjectOptHashMap;

// Track A A3 behavioral test: execute AUTHENTIC VW HMI code converted from J9 ROM.
// IntObjectOptHashMap is an unmodified de.vw.extlibs collection class from the P0369
// platform JXE (tsd.mibstd2.hmi.v2.jxe), converted by our independent ROM->.class converter.
// This harness is our test code; the executed class is authentic Volkswagen HMI bytecode.
public class BehaviorTest {
    public static void main(String[] args) {
        IntObjectOptHashMap m = new IntObjectOptHashMap();
        m.put(1, "RADIO");
        m.put(2, "MEDIA");
        m.put(3, "PHONE");
        Object prev = m.put(2, "MEDIA2");
        boolean ok = m.size() == 3
            && "MEDIA2".equals(m.get(2))
            && "MEDIA".equals(prev)
            && m.get(99) == null
            && m.containsKey(3)
            && !m.containsKey(99)
            && "RADIO".equals(m.remove(1))
            && m.remove(1) == null
            && m.size() == 2
            && !m.isEmpty();
        m.clear();
        ok = ok && m.isEmpty() && m.size() == 0;
        // exercise growth beyond initial capacity
        for (int i = 0; i < 500; i++) m.put(i, "v" + i);
        ok = ok && m.size() == 500 && "v499".equals(m.get(499)) && "v0".equals(m.get(0));
        System.out.println("[behavior] authentic class: " + IntObjectOptHashMap.class.getName());
        System.out.println("[behavior] code source: " + IntObjectOptHashMap.class.getProtectionDomain().getCodeSource().getLocation());
        System.out.println(ok
            ? "[behavior] PASS: authentic VW HMI collection executed correctly on JDK 21 (put/get/remove/clear/growth)"
            : "[behavior] FAIL: authentic HMI collection behavior incorrect");
        if (!ok) System.exit(1);
    }
}
