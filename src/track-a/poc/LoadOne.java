import java.net.*;
import java.nio.file.*;

public class LoadOne {
    public static void main(String[] a) throws Exception {
        URLClassLoader l = new URLClassLoader(new URL[]{Paths.get(a[0]).toUri().toURL()}, LoadOne.class.getClassLoader());
        try {
            Class<?> c = Class.forName(a[1], false, l);
            System.out.println("LOADED OK: " + c.getName() + " fields=" + c.getDeclaredFields().length + " methods=" + c.getDeclaredMethods().length);
        } catch (Throwable t) {
            System.out.println("FAIL: " + t);
            t.printStackTrace(System.out);
        }
    }
}
