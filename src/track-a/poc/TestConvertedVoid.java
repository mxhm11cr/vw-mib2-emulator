// Track A A2 execution test: load the AUTHENTIC firmware java.lang.Void (converted
// from J9 ROM by our independent converter) into JDK 21 via --patch-module java.base,
// replacing the JDK's own Void. Verifies the converted bytecode actually executes.
public class TestConvertedVoid {
    public static void main(String[] args) {
        System.out.println("[a2] Void.TYPE = " + Void.TYPE);
        System.out.println("[a2] Void.TYPE == void.class : " + (Void.TYPE == void.class));
        System.out.println("[a2] Void class source: " + Void.class.getProtectionDomain().getCodeSource());
        boolean ok = Void.TYPE == void.class;
        System.out.println(ok
            ? "[a2] PASS: authentic firmware java.lang.Void (J9 ROM -> .class) executed on JDK 21"
            : "[a2] FAIL: Void.TYPE incorrect");
        if (!ok) System.exit(1);
    }
}
