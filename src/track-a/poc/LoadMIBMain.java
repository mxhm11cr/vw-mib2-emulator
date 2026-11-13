// Track A A4 — load the authentic converted de.vw.mib.MIBMain on JDK 21.
// Loads + links without initializing (initialize=false), mirroring the firmware's -noverify.
// Reports the dependency frontier: which referenced classes are still missing.
public class LoadMIBMain {
    public static void main(String[] args) throws Exception {
        String name = args.length > 0 ? args[0] : "de.vw.mib.MIBMain";
        ClassLoader cl = LoadMIBMain.class.getClassLoader();
        Class<?> c = Class.forName(name, false, cl);
        System.out.println("LOADED: " + c.getName());
        System.out.println("super: " + c.getSuperclass().getName());
        System.out.println("interfaces: " + java.util.Arrays.toString(c.getInterfaces()));
        System.out.println("modifiers: 0x" + Integer.toHexString(c.getModifiers()));
        try {
            System.out.println("declaredFields: " + c.getDeclaredFields().length);
        } catch (Throwable t) {
            System.out.println("declaredFields FAILED: " + t);
        }
        try {
            System.out.println("declaredMethods: " + c.getDeclaredMethods().length);
        } catch (Throwable t) {
            System.out.println("declaredMethods FAILED: " + t);
        }
        try {
            java.lang.reflect.Method m = c.getDeclaredMethod("main", String[].class);
            System.out.println("main method resolved: " + m);
        } catch (Throwable t) {
            System.out.println("main resolve FAILED: " + t);
        }
        if (args.length > 1 && args[1].equals("init")) {
            try {
                Class.forName(name, true, cl);
                System.out.println("INITIALIZED: <clinit> ran without error");
            } catch (Throwable t) {
                System.out.println("INIT frontier (first missing service): " + t);
                Throwable cause = t.getCause();
                int depth = 0;
                while (cause != null && depth++ < 6) {
                    System.out.println("  caused by: " + cause);
                    cause = cause.getCause();
                }
            }
        }
        if (args.length > 1 && args[1].equals("run")) {
            try {
                Class.forName(name, true, cl);
                java.lang.reflect.Method m = c.getDeclaredMethod("main", String[].class);
                System.out.println("invoking main() ...");
                m.invoke(null, (Object) new String[0]);
                System.out.println("main() returned");
            } catch (Throwable t) {
                System.out.println("RUN frontier (first missing service): " + t);
                Throwable cause = t.getCause();
                int depth = 0;
                while (cause != null && depth++ < 8) {
                    System.out.println("  caused by: " + cause);
                    cause = cause.getCause();
                }
            }
        }
    }
}
